#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <functional>
#include <string.h>
#include <random>
#include <climits>
#include <algorithm>

#include <utils.h>
#include <timercpp/timercpp.h>
#include <nngbroker.h>


using namespace std;


NngBroker::NngBroker(void) {
  encryptActivated = false;
}
NngBroker::NngBroker(string jOptions) {
   //cout << "Object is being created" << endl;
   options = json::parse(jOptions);
}
bool NngBroker::listen(string URL)
{
    listen(URL, (int)1);   
}
bool NngBroker::listen(string URL, int threads)
{   
    if (!NngPair::listen(URL)) return false;    
    setBrokerOpts();
    init();
    for(int i=1; i<=threads; i++){
      loopData();    
    }
    //p_startCheckTimeoutConnections();
    if (DEBUGI) fprintf(stderr, "connected broker\n");   
    Online = true; 
    return true;    
}

bool NngBroker::connect(string URL)
{        
    if (!NngPair::connect(URL)) return false;
    setBrokerOpts();
    init();
    loopData();    
    //p_startPing();

    info["name"] = Name;
    string data = info.dump();
    msgHeader header;// = {'c', "connect", ""};
    header.type = 'c'; header.channel = "connect";
    nng::msg msg;
    p_createMsg(&header, &msg, (char *) data.c_str(), data.length()) ;   
    sendMsg(&msg);             
    Online = true;
    return true;
}


void NngBroker::setBrokerOpts() {
    

   if (options.find("ping_interval") == options.end()) options["ping_interval"] = 10000;
   if (options.find("connection_timeout") == options.end()) options["connection_timeout"] = 30000;
   
   //LOAD KEY SET
   if (options.find("key1") != options.end() && options.find("key2") != options.end())  {
       string key1 = options["key1"].get<string>();
       string key2 = options["key2"].get<string>();
       encryption((unsigned char*)key1.c_str(),(unsigned char*)key2.c_str());
   }
   if (Name == "#") Name = gen_random_string(40);
    
}

void NngBroker::init(){
    //initialize maps
    connections = std::make_shared<   std::map<int, Connection>    >
                                    (
                                      std::initializer_list<std::map<int, Connection>::value_type>{}
                                    );

    subscriptions = std::make_shared<    map<string, vector<Subscription>>    >
                                    (
                                      std::initializer_list<   map<string, vector<Subscription>>  ::value_type>{}
                                    );                         

    connectionInfo = std::make_shared<    map<string, json>    >
                                    (
                                      std::initializer_list<   map<string, json>  ::value_type>{}
                                    );                         
}

//new onData
void NngBroker::onData(std::function<void( char *, int)> callback) 
{        
    callbackData = callback;
}

void NngBroker::loopData() 
{        
    status.exchange(1);
    //create native trhead
    std::thread nativeThread = std::thread( [this] {          
        while(true) {                       
            try{
                nng::msg msg = sock->recv_msg();                 
                brokeMsg(&msg);                                                       
            }
            catch( const nng::exception& e ) {                        
                fprintf(stderr, "sendmsg: %s: %s\n", e.who(), e.what());                
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); //delay until reconnect
                return;
            }                   
            if (status ==9) break;
        }                              
    });
    nativeThread.detach();
}

//loop
bool NngBroker::brokeMsg(nng::msg * msg)
{        
    uint32_t  headerSize = msg->body().trim_u32();        
    char * headerData = new char[headerSize];    
    memcpy(headerData,  msg->body().data<char>(), headerSize);

    msg->body().trim(headerSize);
    msgHeader header;    
    pods::InputBuffer in(headerData, headerSize);
    pods::BinaryDeserializer<decltype(in)> deserializer(in);    
    if (deserializer.load(header) != pods::Error::NoError)
    {
        std::cerr << "deserialization error\n";
        return false;
    }
    delete[] headerData;

    //add to connection list
    if (! 
        ((header.encrypted && encryptActivated) || (!header.encrypted && !encryptActivated))
    ) return true; //ignore 

    int conn_id = msg->get_pipe().id();
    char * data;        
    int size;
    bool freeData = false;
    std::vector<unsigned char> edata ;
    if (header.type !='p' && header.encrypted) {
        edata =  p_decrypt(&header, msg->body().data<char>(), msg->body().size());        
        data = (char*)edata.data();
        size = edata.size();
        freeData = true;
    } else {
        data = msg->body().data<char>();        
    }
    switch ( header.type )
      {
        //****************************  
        //connectin stuff
        //****************************
        case 'c': {
            //connection info
            p_updateConnInfo(msg, data);
            break;
         } 
         case 'i': {
            //ping update
            p_updatePing(msg); 
            break;
         }
        
        //****************************  
        //pub sub stuff
        //****************************  
        case 'p':{
            //publish channel            
            //publish local first
            int bodySize =  msg->body().size();
            bool hasPublishedLocal = p_pubContentLocal(header, msg->body().data<char>(),&bodySize);                                        
            //publish remote 
            if (Type == InstanceType::SERVER){ 
                p_pubContentRemote(header, msg->body().data<char>(), &bodySize, conn_id);
            }            
            break;
        }            
        case 's': {
            //subscribe channel
            Subscription newone = {conn_id,  data, false};
            p_addSubscription(header.channel, newone);
            break;
        }            
        case 'u': {
            //unsubscribe channel
            Subscription toremove= {conn_id,  data, false};
            p_removeSubscription(header.channel, &toremove);
            break;
        }
      }
    if (freeData) {
        //malloc used in the encryption
        free(data);         
    }
    
      
   return false;      
}

void NngBroker::encryption(unsigned char * key1, unsigned char * key2){
    int size = 4*(32/3);
    size_t rsize;
    int err;
    unsigned char * key1d = spc_base64_decode(key1, size, &rsize,0, &err);
    unsigned char * key2d = spc_base64_decode(key2, size, &rsize,0, &err);
    encryptor = new ChaCha20Poly1305AEAD(key1d, (int) 32, key2d, (int)32);
    encryptActivated = true;
}

unsigned char * NngBroker::randomKey(){    
    //memcpy(key, (void*)memcpy, 32);
    std::random_device random_device; // create object for seeding
    std::mt19937 engine{random_device()}; // create engine and seed it    
    std::uniform_int_distribution<> dis{1, 255};
    unsigned char *s = (unsigned char*)malloc(32 * sizeof(unsigned char));
    for(int n = 0; n < 32; ++n) {
       s[n] = dis(engine);        
    }            

    return spc_base64_encode(s,32,0);
}

string NngBroker::subscribe(string channel, std::function<void(char *, int)> callback)
{   
    if (State != States::CONNECTED) throw "Not connected!";
    string idSub = gen_random_string(10);
    if (Type == InstanceType::CLIENT)     
    {        
        //client must subscribe to the server
        msgHeader header;
        header.type = 's'; header.channel = channel;// = {'s', channel};
        nng::msg msg;
        p_createMsg(&header, &msg, (char*)idSub.c_str(), 10);    
        sendMsg(&msg);         
        
    }
    //everyone
    Subscription subz;
    subz.local = true;
    subz.sub_id = idSub;
    subz.conn_id = -1;
    subz.callback = callback;// std::make_shared<std::function<void(char *, int)>>( callback );
    p_addSubscription(channel, subz);
    
    return idSub;
    //return "";
}

void NngBroker::unsubscribe(string channel)
{
        unsubscribe(channel, "");   
}

void NngBroker::unsubscribe(string channel, string idSub)
{        
    if (State != States::CONNECTED) throw "Not connected!";
    if (Type == InstanceType::CLIENT)     
    {        
        //client must subscribe to the server
        msgHeader header;
        header.type = 'u'; header.channel = channel;
        nng::msg msg;
        p_createMsg(&header, &msg, (char *)idSub.c_str(), 10);    
        sendMsg(&msg);         
        
    }
    //everyone
    Subscription subz;
    subz.local = true;
    subz.sub_id = idSub;
    subz.conn_id = -1;
    p_removeSubscription(channel, &subz);
}

void NngBroker::publish(string channel, char* data, int size)
{   //p = publish
    if (State != States::CONNECTED) throw "Not connected!";
    msgHeader header; header.type = 'p'; header.channel = channel;    
    

    //**********************************************
    //publish local before encryption
    //**********************************************
    bool hasPublishedLocal = p_pubContentLocal(header, data, &size);    
    
    //**********************************************
    //prepare to send remote
    //**********************************************
    //encrypt here
    bool freeData = false;
    std::vector<unsigned char> edata;
    if (encryptActivated ) {
        unsigned char * dstData;
        int dstSize;
        edata =  p_encrypt(&header, data, size);        
        data = (char*)edata.data();        
        size = edata.size();
    }     

    //send to remote subscriptions
    if (Type == InstanceType::CLIENT)     
    {        
        nng::msg msg;
        p_createMsgRaw(&header, &msg, data, size);// create encrypted   
        sendMsg(&msg);               
    } else {
        //publish for remote  subscriptions         
        p_pubContentRemote(header, data, &size, -1);        
    }   
    
    if (freeData) {
        //malloc used in the encryption
        //free(data);         
        //edata.resize(0);
    }

}
    

//**************************************************************************************
//private helpers
//**************************************************************************************

void NngBroker::p_updateConnInfo(nng::msg * msg, char * data) {  
    json item = json::parse(data);
    int conn_id = msg->get_pipe().id();
    p_checkConnection(msg);
    
    if (!connections->count(conn_id)) return;    

    item["conn_id"] = conn_id;
    string name = item["name"].get<string>();    
    if (!connectionInfo->count(name)) {        
        connectionInfo->insert(pair<string, json>(name,item));
    } else {
        connectionInfo->at(name) = item;
    }            
}

bool NngBroker::p_pubContentLocal(msgHeader header, char* data, int * size){
    //std::cout << Type << std::endl;   
    string channel = header.channel;
    if (!subscriptions->count(channel)) {
        return false;
    }    
    bool published = false;
    std::vector<unsigned char> edata;
    if (header.encrypted) {
        edata =  p_decrypt(&header, data, *size);        
        data = (char*)edata.data();
        *size = edata.size();
    }
    
    for (int i =0; i < subscriptions->at(channel).size(); ++i) {
        //local subscription            
        if (!subscriptions->at(channel)[i].local) continue; //only check locals
        if (header.sub_id == subscriptions->at(channel)[i].sub_id || header.sub_id.length() == 0) {                
            subscriptions->at(channel)[i].callback(data, *size);            
            published = true;
        }  
    }    
    return published;
}

bool NngBroker::p_pubContentRemote(msgHeader header, char* data, int * size, int sender_conn_id){
    //std::cout << Type << std::endl;   
    string channel = header.channel;
    if (!subscriptions->count(channel)) {
        return false;
    }    
    bool published = false;    
    //msgHeader headerThis = header;    
    nng::msg msg;
    for (int i =0; i < subscriptions->at(channel).size(); ++i) {        
        if (subscriptions->at(channel)[i].local) continue; //only check remotes        
        //send remote subscription                    
        int conn_id = subscriptions->at(channel)[i].conn_id;        
        if (sender_conn_id != -1){
            if (sender_conn_id == conn_id) continue; //sender will publish localy, dont send to client 
            if (!connections->count(conn_id)) {
                p_removeSubscription(channel, &subscriptions->at(channel)[i]);
                continue;
            }
        }
        //forward content        
        header.sub_id = subscriptions->at(channel)[i].sub_id;
        p_createMsgRaw(&header, &msg, data, *size);
        if (sender_conn_id != -1) msg.set_pipe(connections->at(conn_id).source);   //if conn_id=-1 just send to the server        
        sendMsg(&msg);                 
        published = true;
    }
    
    return published;
}


void NngBroker::p_addSubscription(string channel, Subscription subz){
    if (!subscriptions->count(channel)) {        
        subscriptions->insert(pair<string,vector<Subscription>>(channel,vector<Subscription>()));
    }    
    subscriptions->at(channel).push_back(subz);
    //fprintf(stderr, "subscribed--------------------\n");   
}

void NngBroker::p_removeSubscription(string channel, Subscription * subz){ 
    if (!subscriptions->count(channel)) return;
    for (int i =0; i < (int) subscriptions->at(channel).size(); ++i) {
        
        if (subscriptions->at(channel)[i].conn_id == subz->conn_id){
            if (subz->sub_id.length() > 1) {
                if (subscriptions->at(channel)[i].sub_id == subz->sub_id){
                    subscriptions->at(channel)[i].callback = NULL;
                    subscriptions->at(channel).erase(subscriptions->at(channel).begin()+i);
                }
            } else {
                //remove
                subscriptions->at(channel)[i].callback = NULL;
                subscriptions->at(channel).erase(subscriptions->at(channel).begin()+i);
            }
            
        }
    }        
}

void NngBroker::p_updatePing(nng::msg *msg){
    int id =  msg->get_pipe().id();
    if (connections->count(id)) {
        fprintf(stderr, "update ping %d", id);
        connections->at(id).lastPing = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void NngBroker::p_startPing(){
    Timer * timerPing = new Timer();

    int interval = options["ping_interval"].get<int>();    
    timerPing->setInterval([&]() {       
        fprintf(stderr, "sending pingn--------------------\n");                  
        msgHeader header;
        header.type = 'i'; header.channel = "ping";
        nng::msg msg;
        p_createMsg(&header, &msg, (char*)"ping", 4) ;
        sendMsg(&msg);                
                               
        if (status ==9) timerPing->stop();;        
    }, interval); 
}

void NngBroker::p_startCheckTimeoutConnections(){
    Timer * timerCheckConn = new Timer();    
    int interval = options["connection_timeout"].get<int>()/2;    
    map<int, Connection>::iterator it;
    int now=0;
    timerCheckConn->setInterval([&]() {        
        int timeout = options["connection_timeout"].get<int>()/1000;
        now = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();             
        for ( it = connections->begin(); it != connections->end(); it++ )
        {
            
            if (now - it->second.lastPing >= (timeout)) { //seconds                
                fprintf(stderr, "remove connection %d--of %d---\n", now - it->second.lastPing, timeout);
                p_removeConnection(it->first);
            }                   
        }        
        if (status ==9) timerCheckConn->stop();
    }, interval); 
}

void NngBroker::p_checkConnection(nng::msg *msg){
    nng::pipe_view source = msg->get_pipe();
    int id = source.id();
    if (!connections->count(id)) {
        Connection newpeer;
        newpeer.source = msg->get_pipe();
        newpeer.conn_id = id;
        newpeer.lastPing = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count();
        connections->insert(std::pair<int,Connection>(id,newpeer));
    }
}

void NngBroker::p_removeConnection(int conn_id){    
    //fprintf(stderr, "removing %d", conn_id);
    if (!connections->count(conn_id)) return;
    //close and remove connection
    nng_pipe_close(connections->at(conn_id).source.get());
    connections->erase(conn_id);  

    //remove connection info
    map<string, json>::iterator it;
    for ( it = connectionInfo->begin(); it != connectionInfo->end(); it++ )
    {
        if (it->second["conn_id"].get<int>() == conn_id){
            connectionInfo->erase(it->first);  
            break;
        }        
    }
}
   
void NngBroker::p_createMsgRaw( msgHeader * header, nng::msg *msgto, char* data, int size) {    
    *msgto = nng::make_msg(0);        

    //SERIALIZE HEADER       
    pods::ResizableOutputBuffer out;
    pods::BinarySerializer<decltype(out)> serializer(out);
    if (serializer.save(*header) != pods::Error::NoError)
    {
        return ;      
    }

    //put header
    uint32_t sizedata = out.size();
    msgto->body().append_u32(sizedata); 
    msgto->body().append(nng::view(out.data(),out.size())); 

    //put data
    if (data !=NULL) {       
        msgto->body().append(nng::view(data, size)); 
    }    
 //   return msgto;
}


void NngBroker::p_createMsg( msgHeader * header, nng::msg *msgto, char* data, int size) {
    if (!encryptActivated ) {
        return p_createMsgRaw(header, msgto, data, size);
    } 
    
    std::vector<unsigned char> edata =  p_encrypt(header, data, size);            

    p_createMsgRaw(header, msgto, (char*)edata.data(), edata.size());
    //edata.resize(0);
}
     
std::vector<unsigned char>  NngBroker::p_decrypt(msgHeader * header, char* data, int size) {    
    int aad_pos = 0;    
    int dsize = size-POLY1305_TAGLEN;     
    //fprintf(stderr, "decrypt %d\n", dsize);    
    std::vector<unsigned char> plain_buf(dsize, 0);
    
    bool res = encryptor->Crypt(header->iv1 , header->iv2, aad_pos, plain_buf.data(),  plain_buf.size(), (unsigned char*) data, size, false);    
    return plain_buf;
}

std::vector<unsigned char> NngBroker::p_encrypt( msgHeader * header, char * data, int size) {
    header->encrypted = true;
    header->iv1 = gen_random_number();
    header->iv2  = gen_random_number();
    
    int dsize = size+POLY1305_TAGLEN;     
    //fprintf(stderr, "encrypt %d\n", dsize);    
    std::vector<unsigned char> ciphertext_buf(dsize, 0);
    
    int aad_pos = 0;
    bool res = encryptor->Crypt(header->iv1 , header->iv2, aad_pos, ciphertext_buf.data(), ciphertext_buf.size(), (unsigned char*) data, size, true);    
    return ciphertext_buf;
}




  /*
nng::msg * NngBroker::p_Encrypt( msgHeader header, char* data, int size) {

    }
    */