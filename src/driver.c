
#include <pthread.h>
#include <atomic>
#include <unistd.h>
#include <iostream>

#include "driver.h"
#include "hiredis/hiredis.h"

#define REDIS_UNIX_SOCKET "/var/run/redis/redis-server.sock"

static DrvManer* _drvManer;
static std::atomic<bool> _isOpen(false);
static redisContext * _db;

InterfaceHandle _dio1;
InterfaceHandle _dio2;
InterfaceHandle _ad1;
InterfaceHandle _da1;
InterfaceHandle _serial1;
InterfaceHandle _com1;

//send digital out
void send_digital(int tag, bool value) {

    if(tag==1) {//DO DIO1
        auto db = redisConnectUnix(REDIS_UNIX_SOCKET);
        auto reply = (redisReply*)redisCommand(db, "LPUSH %s %s", "*DIO1", value ? "1":"0");
        if(reply==NULL) {
            _drvManer->logInfo("error1");
        }
        freeReplyObject(reply);
        redisFree(db);
    }
    else if(tag==2) {//DI DIO2
        //_drvManer->logInfo("2");
        return;
    }
}

//send analog out
void send_analog(int tag, int value) {
    if(tag==1) {//AD1
        return;
    }
    else if(tag==2) {//DA1 send value
        auto db = redisConnectUnix(REDIS_UNIX_SOCKET);
        auto reply = (redisReply*)redisCommand(db, "LPUSH %s %d", "*DA1", value);
        if(reply==NULL) {
            _drvManer->logInfo("error2");
        }
        freeReplyObject(reply);
        redisFree(db);
    }
}

//send stream data out
void send_data(int tag, const char* buff, unsigned int buff_len, json* option) {
    auto data = json::object();
    data["v"] = std::string(buff, buff_len);
    if(option) {
        data["o"] = *option;
    }
    auto new_buff = json::to_msgpack(data);

    auto db = redisConnectUnix(REDIS_UNIX_SOCKET);
    if(tag==1) {//Serial1
        auto reply = (redisReply*)redisCommand(db,"LPUSH %b %b", "*Serial1_OUT", (size_t)12, new_buff.data(), (size_t)new_buff.size());
        if(reply==NULL) {
            _drvManer->logInfo("error3");
        }
        freeReplyObject(reply);
    }
    else if(tag==2) {//COM1
        auto reply = (redisReply*)redisCommand(db,"LPUSH %b %b", "*COM1_OUT", (size_t)9, new_buff.data(), (size_t)new_buff.size());
        if(reply==NULL) {
            _drvManer->logInfo("error4");
        }
        freeReplyObject(reply);
    }
    redisFree(db);
    // _drvManer->logInfo("sended: ");
    // _drvManer->logInfo(buff);
}

//recv stream data
void recv_data(int tag) {
    char port[12];
    InterfaceHandle h;

    if(tag==1) {//Serial1
        sprintf(port, "*%s", "Serial1_IN");
        h = _serial1;
    }
    else if(tag==2) {//COM1
        sprintf(port, "*%s", "COM1_IN");
        h = _com1;
    }
    
    redisReply* reply = (redisReply*)redisCommand(_db, "RPOP %s", port);
    if(reply==nullptr || reply->type!=REDIS_REPLY_STRING) {
        if(reply) {
            freeReplyObject(reply);
            reply = nullptr;
        }
    } else {
        auto buff = string(reply->str, reply->len);
        auto data = json::from_msgpack(buff);
        auto str = data["v"].get<string>();
        json* option = data.contains("o") ? &data["o"] : nullptr;
        _drvManer->recvedData(h, str.c_str(), str.size(), option);
        freeReplyObject(reply);
        reply = nullptr;
        // _drvManer->logInfo("recved: ");
        // _drvManer->logInfo(str.c_str());
    }
}

//tick all data
void tick(unsigned long long timer) {

    if(!_isOpen) {
        return;
    }
    //_drvManer->logInfo("Tick");
    
    //read DI
    redisReply *reply = nullptr;
    //_drvManer->logInfo("aaaa");
    reply = (redisReply*)redisCommand(_db, "RPOP %s", "*DIO2");
    //_drvManer->logInfo("bbbb");
    if(reply==nullptr || reply->type!=REDIS_REPLY_STRING) {
        if(reply) {
            freeReplyObject(reply);
            reply = nullptr;
        }
    }else {
        bool v = (reply->len==1 && reply->str[0]=='1');
        _drvManer->recvedDigital(_dio2, v);
        if(reply) {
            freeReplyObject(reply);
            reply = nullptr;
        }        
    }

    //read AD
    reply = nullptr;
    reply = (redisReply*)redisCommand(_db, "RPOP %s", "*AD1");
    if(reply==nullptr || reply->type!=REDIS_REPLY_STRING) {
        if(reply) {
            freeReplyObject(reply);
            reply = nullptr;
        }
    } else {
        _drvManer->recvedAnalog(_ad1, atoi(reply->str));
        if(reply) {
            freeReplyObject(reply);
            reply = nullptr;
        }
    }


    //read Serial1
    recv_data(1);

    //read COM1
    recv_data(2);

}

void *loop(void* arg)
{

    auto db = redisConnectUnix(REDIS_UNIX_SOCKET);

    for(;;)
    {
        if(!_isOpen) {
            redisFree(db);
            break;
        }

        redisReply *reply1;
        redisReply *reply2;

        //read DO then write to DI
        for(;;) {
            reply1 = (redisReply*)redisCommand(db, "RPOP %s", "*DIO1");
            if(reply1==NULL) {
                break;
            }
            if(reply1->type!=REDIS_REPLY_STRING) {
                freeReplyObject(reply1);
                break;
            } else {
                reply2 = (redisReply*)redisCommand(db, "LPUSH %s %s", "*DIO2", reply1->str);
                freeReplyObject(reply1);
                if(reply2!=NULL) {
                    freeReplyObject(reply2);
                }
            }
        }

        //read DA1 then write to AD1
        for(;;) {
            reply1 = (redisReply*)redisCommand(db, "RPOP %s", "*DA1");
            if(reply1==NULL) {
                break;
            }
            if(reply1->type!=REDIS_REPLY_STRING) {
                freeReplyObject(reply1);
                break;
            } else {
                reply2 = (redisReply*)redisCommand(db, "LPUSH %s %s", "*AD1", reply1->str);
                freeReplyObject(reply1);         
                if(reply2!=NULL) {
                    freeReplyObject(reply2);
                }       
            }

        }


        //serial1 out to in
        for(;;) {
            reply1 = (redisReply*)redisCommand(db, "RPOP %s", "*Serial1_OUT");
            if(reply1==NULL) {
                break;
            }
            if(reply1->type!=REDIS_REPLY_STRING) {
                freeReplyObject(reply1);
                break;
            } else {
                reply2 = (redisReply*)redisCommand(db,"LPUSH %b %b", "*Serial1_IN", (size_t)11, reply1->str, reply1->len);
                freeReplyObject(reply1);
                if(reply2!=NULL) {
                    freeReplyObject(reply2);
                }
            }
        }

        //COM1 out to in
        for(;;) {
            reply1 = (redisReply*)redisCommand(db, "RPOP %s", "*COM1_OUT");
            if(reply1==NULL) {
                break;
            }
            if(reply1->type!=REDIS_REPLY_STRING) {
                freeReplyObject(reply1);
                break;
            } else {
                reply2 = (redisReply*)redisCommand(db,"LPUSH %b %b", "*COM1_IN", (size_t)8, reply1->str, reply1->len);
                freeReplyObject(reply1);
                if(reply2!=NULL) {
                    freeReplyObject(reply2);
                }
            }
        }

        usleep(5000);
    }
    
    pthread_exit(NULL);

}


void tick1(unsigned long long timer) {
    if(!_isOpen) {
        return;
    }

    static int i=0;
    _drvManer->recvedAnalog(_ad1, i%100);

    static bool b = true;
    bool new_b = (i%2==0);
    if(new_b!=b) {
        _drvManer->recvedDigital(_dio2, new_b);
        b=new_b;
    }

    ++i;
}


//flush io data
void flush_data(int tag) {
    auto db = redisConnectUnix(REDIS_UNIX_SOCKET);
    
    redisReply* reply;
    if(tag==1) {//Serial1
        reply = (redisReply*)redisCommand(db,"LTRIM %s 1 0", "*Serial1_OUT");
        freeReplyObject(reply);
        reply = (redisReply*)redisCommand(db,"LTRIM %s 1 0", "*Serial1_IN");
        freeReplyObject(reply);
    }
    else if(tag==2) {//COM1
        reply = (redisReply*)redisCommand(db,"LTRIM %s 1 0", "*COM1_OUT");
        freeReplyObject(reply);
        reply = (redisReply*)redisCommand(db,"LTRIM %s 1 0", "*COM1_IN");
        freeReplyObject(reply);
    }
    redisFree(db);
}

//create interface of card
void card_create(int tag, json& interfaces_config) {
    for(auto& inf : interfaces_config) {
        auto inf_id = inf["interface"].get<string>();
        auto inf_name = inf["name"].get<string>().c_str();
        if(inf_id == "DIO1") {
            _dio1 = _drvManer->registerDigitalInterface(inf_name, 1, send_digital);
        } else if(inf_id == "DIO2") {
            _dio2 = _drvManer->registerDigitalInterface(inf_name, 2, send_digital);
        } else if(inf_id=="AD1") {
            _ad1 = _drvManer->registerAnalogInterface(inf_name, 1, nullptr);
        } else if(inf_id == "DA1") {
            _da1 = _drvManer->registerAnalogInterface(inf_name, 2, send_analog);
        } else if(inf_id == "Serial1") {
            _serial1 = _drvManer->registerDataInterface(inf_name, 1, send_data, recv_data, flush_data);
        } else if(inf_id == "COM1") {
            _com1 = _drvManer->registerDataInterface(inf_name, 2, send_data, recv_data, flush_data);
        }
    }
}


void card_open(int tag) {
    _isOpen = true;
    
    pthread_t tid;
    pthread_create(&tid,NULL,loop,NULL);
}

void card_close(int tag) {
    _isOpen = false;
}

void e_initial(DrvManer* drvManer) {
    _isOpen = false;
    _drvManer = drvManer;

    _db = redisConnectUnix(REDIS_UNIX_SOCKET);
    //_db = redisConnect("127.0.0.1", 6379);
    if (_db == NULL || _db->err) {
        char buff[800];
        if (_db) {
            sprintf(buff, "Error: %s\n", _db->errstr);
            // handle error
        } else {
            sprintf(buff, "Can't allocate redis context\n");
        }
        _drvManer->logError(buff);
    }

    _drvManer->registerCard("VirtualDevice", 0, card_create, card_open, card_close, tick);
}

void e_release() {
    if(_db!=NULL) {
        redisFree(_db);
    }
}