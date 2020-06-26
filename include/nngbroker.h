#ifndef NNGBROKER_INCLUDED
#define NNGBROKER_INCLUDED

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <functional>

#include <pods/include/pods/pods.h>
#include <pods/include/pods/binary.h>
#include <pods/include/pods/buffers.h>

#include <crypto/spc_b64.h>
#include <crypto/chacha_poly_aead.h>
#include <crypto/poly1305.h>

#include <nngpair.h>



using namespace std;
using json = nlohmann::json;

typedef struct {
	nng::pipe_view source;
    int conn_id=0;
	int lastPing=0;
    bool authorized = false;
} Connection ;

typedef struct {	
    int conn_id=0;
	string sub_id;
    bool local = false;
    std::function<void(char *, int)> callback = NULL;    
} Subscription ;

struct msgHeader
{
    char type = '#'; 
    string channel = "#";
    string sub_id;
    map<string, string> info;
    //char msgtype = 'p'; //p=publishall o=publishonce r=request e=requestonce //problem with local publish
    
    //auto
    bool encrypted = false; 
    uint32_t iv1 = 0;
    uint32_t iv2 = 0;
    PODS_SERIALIZABLE(
        1, // this is version
        PODS_MDR(type),
        PODS_MDR(channel),
        PODS_MDR(info),
        PODS_MDR(encrypted),
        PODS_OPT(sub_id),
        PODS_OPT(iv1),
        PODS_OPT(iv2)
    )         
};


typedef std::function<void(char *, int)> CallbacksFuncs;

class NngBroker : public NngPair
{
    protected:        
                         
        std::shared_ptr< map<string, vector<Subscription>>> subscriptions;
        std::shared_ptr<map<int, Connection>> connections;
        std::shared_ptr<map<string, json>> connectionInfo;             
        
        function<void(char *, int)> callbackData = NULL;
        void p_updatePing(nng::msg *msg);
        void p_startPing();
        void p_startCheckTimeoutConnections();
        void p_checkConnection(nng::msg *msg); 
        void p_removeConnection(int conn_id);
        void p_updateConnInfo(nng::msg * msg, char * data);        
        void loopData();
        void init();

        ChaCha20Poly1305AEAD * encryptor;        
        std::vector<unsigned char> p_decrypt(msgHeader * header, char* data, int size);
        std::vector<unsigned char> p_encrypt( msgHeader * header, char * data, int size);
        bool encryptActivated =false;
       

    private:    
        void p_addSubscription(string channel, Subscription subz);
        void p_removeSubscription(string channel, Subscription * subz);        
        void p_createMsgRaw(msgHeader * header, nng::msg *msgto, char* data, int size);
        void p_createMsg(msgHeader * header,  nng::msg *msgto, char* data, int size);
        bool p_pubContentLocal(msgHeader header, char* data, int * size);
        bool p_pubContentRemote(msgHeader header, char* data, int * size, int sender_conn_id);
        bool p_pubContent(msgHeader header, char* data, int size, int sender_conn_id, bool isLocal);         
        bool brokeMsg(nng::msg * msg);  //parse message  
        void setBrokerOpts(); 

        

    public:                           
        string Name ="#";
        json info;        
        //functions
        NngBroker(string jOptions);    
        NngBroker(void);
        

        bool listen(string URL);        
        bool listen(string URL, int threads);
        bool connect(string URL); 
        void onData(std::function<void( char *, int)> callback);
        string subscribe(string channel, std::function<void(char *, int)> callback);
        void unsubscribe(string channel, string idSub);
        void unsubscribe(string channel);
        void publish(string channel, char* data, int size);  
        void encryption(unsigned char * key1, unsigned char * key2);            
        unsigned char * randomKey();
        
}; 

#endif //NNGBROKER_INCLUDED