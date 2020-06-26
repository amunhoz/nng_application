#pragma pack(pop)
//#include <napi.h>
#include "lib.h"

using namespace Napi;

Napi::FunctionReference LibNode::constructor;

//constructor
//*********************************************************************
LibNode::LibNode(const Napi::CallbackInfo& info) : ObjectWrap(info) {
    Napi::Env env = info.Env();

    
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Wrong number of arguments")
          .ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsString()) {
        Napi::TypeError::New(env, "You need to name yourself")
          .ThrowAsJavaScriptException();
        return;
    }
    
    string config = info[0].As<Napi::String>().Utf8Value();
    
    socket = NngBroker(config);
}

//methos
//*********************************************************************
void LibNode::OnData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();    
    tread_Callback = std::make_shared<Napi::ThreadSafeFunction>(
        ThreadSafeFunction::New(env,  info[0].As<Function>(), "Callback",  0, 100)     
    );    
}

Napi::Value LibNode::Listen(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    string address = info[0].As<Napi::String>().Utf8Value();
    bool result = socket.listen(address);    
    return Napi::Boolean::New(env, socket.Online);
}

Napi::Value LibNode::Connect(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    bool result =false;
    
    string address = info[0].As<Napi::String>().Utf8Value();
    result = socket.connect(address);

    return Napi::Boolean::New(env, socket.Online);
}
    //char * tbuff;
    //int tsize;
Napi::Value LibNode::Subscribe(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    //auto envc = std::make_shared< Napi::CallbackInfo>(env);

    Napi::Value channeln = info[0].As<Napi::String>();
    std::function<void(char *, int )> callBackCpp([env, channeln, this](char * buff, int size){    
        tread_Callback->BlockingCall([&]( Napi::Env env, Function jCallback) {                   
            Napi::Buffer<char> data = Napi::Buffer<char>::New(env, buff,  size);                           
            jCallback.Call( {channeln, data });
        }); 
    });
    string channel = info[0].As<Napi::String>().Utf8Value();
    string result = socket.subscribe(channel, callBackCpp);    
    
    //save to release later
    //subzCallbacks.insert(pair<string, cBackInfo*>(result, &infoFunc));    

    return Napi::String::New(env, result);
}

Napi::Value LibNode::Unsubscribe(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    bool result ;
    string channel = info[0].As<Napi::String>().Utf8Value();
    string subid = info[1].As<Napi::String>().Utf8Value();
    socket.unsubscribe(channel, subid);
    
    auto search = subzCallbacks.find(subid);
    if (search != subzCallbacks.end()) {    
        subzCallbacks.erase(search);
    }    
    return Napi::Boolean::New(env, true);
}

Napi::Value LibNode::Publish(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    bool result ;        
    try {
        string channel = info[0].As<Napi::String>().Utf8Value();    
        Napi::Buffer<char> buff = info[1].As<Napi::Buffer<char>>(); 
        int size =  buff.Length();
        socket.publish(channel, buff.Data(), size);
        result = true;
    }catch (const std::exception& e)
    {
        fprintf(stderr, "%s\n", e.what());
        result = false;
        //Error::New(info.Env(), "error general.").ThrowAsJavaScriptException();
    }    
    return Napi::Boolean::New(env, result);
}
/*
Napi::Function LibNode::GetClass(Napi::Env env) {
    return DefineClass(env, "nngbroker", {
        LibNode::InstanceMethod("listen", &LibNode::Listen),
        LibNode::InstanceMethod("connect", &LibNode::Connect),
        LibNode::InstanceMethod("subscribe", &LibNode::Subscribe),
        LibNode::InstanceMethod("unsubscribe", &LibNode::Unsubscribe),
        LibNode::InstanceMethod("publish", &LibNode::Publish),
    });
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::String name = Napi::String::New(env, "nngbroker");
    exports.Set(name, LibNode::GetClass(env));
    return exports;
}
*/

Napi::Object LibNode::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func =
      DefineClass(env,
                  "nngbroker",
                  {
                        InstanceMethod("listen", &LibNode::Listen),
                        InstanceMethod("connect", &LibNode::Connect),
                        InstanceMethod("subscribe", &LibNode::Subscribe),
                        InstanceMethod("unsubscribe", &LibNode::Unsubscribe),
                        InstanceMethod("publish", &LibNode::Publish),
                        InstanceMethod("ondata", &LibNode::OnData),
                   });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();  
  exports.Set("nngbroker", func);
  return exports;
}

// Initialize native add-on
Napi::Object Init (Napi::Env env, Napi::Object exports) {
    LibNode::Init(env, exports);
    return exports;
}
// Register and initialize native add-on
//NODE_API_MODULE(libnngcom, Init)
NODE_API_MODULE(addon, Init)