#ifndef NNGBROKERJS_H
#define NNGBROKERJS_H

#define NAPI_EXPERIMENTAL
#include <napi.h>
#include <nngbroker.h>

using namespace Napi;

typedef struct {	
    string channel;
    int size =0;
	char * data;    
} dataCallBack ;

typedef struct {	
    std::string event;    
} cBackInfo ;

class LibNode : public Napi::ObjectWrap<LibNode>
{
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    LibNode(const Napi::CallbackInfo&);
    //~LibNode() override = default;


private:
    
    static Napi::FunctionReference constructor;

    Napi::Value Listen(const Napi::CallbackInfo&);
    Napi::Value Connect(const Napi::CallbackInfo&);
    Napi::Value Subscribe(const Napi::CallbackInfo&);
    Napi::Value Unsubscribe(const Napi::CallbackInfo&);    
    Napi::Value Publish(const Napi::CallbackInfo&);
    void OnData(const Napi::CallbackInfo& info);

    NngBroker socket;    
    map<string, cBackInfo*> subzCallbacks;        
    std::shared_ptr<Napi::ThreadSafeFunction> tread_Callback;  
};

#endif