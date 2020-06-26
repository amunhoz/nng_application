#ifndef NNGPAIR_INCLUDED
#define NNGPAIR_INCLUDED

#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>

#include <nngpp/nngpp.h>
#include <nngpp/protocol/pair1.h>

#include <nngpp/platform/platform.h>
#include <nngpp/option.h>

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class NngPair
{        
    protected:        
        std::shared_ptr<nng::socket> sock ;        
        string type = "#";
        static std::atomic<int> status;      
        bool sendMsg(nng::msg * msg);
        bool intermediate(nng::msg * msg);    
        void setSockOpts();       
        json options;
        mutex * semaphore;
        bool DEBUGI=false;

    private: 
        void setPairOpts();        
        

    public:   
        bool Online = false;

        //functions
        NngPair(string jOptions);
        NngPair(void);
        
        bool listen(string URL);        
        bool connect(string URL); 
        
        void onData(std::function<void(char *, int)> callback);
        bool send(char * buff, int size);  
        void close(); 
        enum States
        {
                WAITING,
                CONNECTED,
                ERROR,
                CLOSED
        };
        States State = States::WAITING;
         enum InstanceType
        {
                NOT_DEFINED,
                SERVER,
                CLIENT                
        };
        InstanceType Type = InstanceType::NOT_DEFINED;

}; 

#endif // NNGPAIR_INCLUDED