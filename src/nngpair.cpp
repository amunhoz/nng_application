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
#include<bits/stdc++.h>

#include <nngpair.h>

using namespace std;

NngPair::NngPair(void) {
    semaphore = new std::mutex();
}

NngPair::NngPair(string jOptions) {
   options = json::parse(jOptions);
}

std::atomic<int> NngPair::status{-1};

bool NngPair::listen(string URL)
{            
    Type = InstanceType::SERVER;
    nng::socket sockx = nng::pair::v1::open_poly();
    sock = std::make_shared<nng::socket>( sockx.release() );
    setPairOpts();
    setSockOpts();
    try {	  
        sock->listen(URL.c_str());
        status.exchange(0);
        State = States::CONNECTED;
        if (DEBUGI) fprintf(stderr, "connected pair\n");    
        return true;
	}
	catch( const nng::exception& e ) {
        fprintf(stderr, "listen: %s: %s\n", e.who(), e.what());
        State = States::ERROR;
        return false;
    }    
}

bool NngPair::connect(string URL)
{        
    
    nng::socket sockx = nng::pair::v1::open();
    sock = std::make_shared<nng::socket>( sockx.release() );

    //sock = std::make_shared<nng::socket>( nng::pair::v1::open() );        
    Type = InstanceType::CLIENT;
    setPairOpts();
    setSockOpts();
    
    //nng::set_opt_reconnect_time_min( sock, 2 );      
    try {	    
        sock->dial(URL.c_str());
        status.exchange(0);
        State = States::CONNECTED;
        return true;
	}
	catch( const nng::exception& e ) {
        fprintf(stderr, "connect %s: %s\n", e.who(), e.what());
        State = States::ERROR;
        return false;
    }    
}

void NngPair::setPairOpts() {
   //cout << "Object is being created" << endl;
   if (options.find("send_buffer") == options.end()) options["send_buffer"] = 6000; //max 8192
   if (options.find("recv_buffer") == options.end()) options["recv_buffer"] = 6000; //max 8192
   //if (options.find("recv_timeout") == options.end()) options["recv_timeout"] = 0; //ms
   //if (options.find("send_timeout") == options.end()) options["send_timeout"] = 0; //ms
   if (options.find("recv_size_max") == options.end()) options["recv_size_max"] =0; //no limit
   if (options.find("reconnect_time_min") == options.end()) options["reconnect_time_min"] = 100; //ms
   if (options.find("reconnect_time_max") == options.end()) options["reconnect_time_max"] = 1000;  //ms
    
}
void NngPair::setSockOpts() {
    sock->set_opt_int(nng::to_name(nng::option::send_buffer), 3000);
    sock->set_opt_int(nng::to_name(nng::option::recv_buffer), 3000);

    //sock->set_opt_int(nng::to_name(nng::option::send_buffer), options["send_buffer"].get<int>());
    //sock->set_opt_int(nng::to_name(nng::option::recv_buffer), options["recv_buffer"].get<int>());
    //sock->set_opt_ms(nng::to_name(nng::option::recv_timeout), (nng_duration) options["recv_timeout"].get<int>());
    //sock->set_opt_ms(nng::to_name(nng::option::send_timeout), (nng_duration) options["send_timeout"].get<int>());    
    sock->set_opt_ms(nng::to_name(nng::option::reconnect_time_min), (nng_duration)options["reconnect_time_min"].get<int>());
    sock->set_opt_ms(nng::to_name(nng::option::reconnect_time_max),  (nng_duration)options["reconnect_time_max"].get<int>());
     if (Type == InstanceType::SERVER)     
    {        
        sock->set_opt_size(nng::to_name(nng::option::recv_size_max), options["recv_size_max"].get<int>());        
    }    
}


void NngPair::onData(std::function<void( char *, int)> callback) 
{        
    status.exchange(1);
    //create native trhead
    std::thread nativeThread = std::thread( [this, &callback] {                  
        while(true) {
            if (status ==-1) {
             //   std::this_thread::sleep_for(std::chrono::milliseconds(50));
             //   continue;
            }  
            nng::msg msg = sock->recv_msg();              
            char* data= msg.body().data< char>();             
            callback(data, (int) msg.body().size());                            
            
            if (status ==9) break;
        }                              
    });
    nativeThread.detach();
}


bool NngPair::send(char * buff, int size)
{    
    if (State != States::CONNECTED) throw "Not connected!";
    auto msg = nng::make_msg(0);        
    msg.body().append(nng::view(buff,size)); 
    sendMsg(&msg);
    return true;
}

bool NngPair::sendMsg(nng::msg * msg)
{    
    try {	    
        semaphore->lock();
        sock->send( std::move(*msg) );
        semaphore->unlock();
        return true;
	}
	catch( const nng::exception& e ) {        
        fprintf(stderr, "sendmsg: %s: %s\n", e.who(), e.what());        
        return false;
    }        
}


void NngPair::close()
{        
    //sock->release();
    State = States::CLOSED;
    status.exchange(9);
}


    

