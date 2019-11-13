
#include "driver.h"
#include "hiredis/hiredis.h"

#define REDIS_UNIX_SOCKET "/var/run/redis/redis-server.sock"

static DrvManer* _drvManer;
static bool _isOpen;
static redisContext * _db;

InterfaceHandle _dio1;
InterfaceHandle _dio2;
InterfaceHandle _ad1;
InterfaceHandle _da1;
InterfaceHandle _serial1;
InterfaceHandle _com1;

void send_digital(int tag, bool value) {
    if(tag==1) {//DO DIO1
        auto reply = (redisReply*)redisCommand(_db, "LPUSH %s %c", "*DIO1", value ? '1':'0');
        
        freeReplyObject(reply);
    }
    else if(tag==2) {//DI DIO2
        return;
    }
}

void send_analog(int tag, int value) {
    if(tag==1) {//AD1
        return;
    }
    else if(tag==2) {//DA1 send value
        auto reply = (redisReply*)redisCommand(_db, "LPUSH %s %d", "*DA1", value);
        
        freeReplyObject(reply);
    }
}

void send_data(int tag, const char* buff, unsigned int buff_len, json* option) {
    if(tag==1) {//Serial1
        auto reply = (redisReply*)redisCommand(_db,"LPUSH %b %b", "*Serial1_OUT", (size_t)12, buff, (size_t)buff_len);

        freeReplyObject(reply);
    }
    else if(tag==2) {//COM1
        auto reply = (redisReply*)redisCommand(_db,"LPUSH %b %b", "*COM1_OUT", (size_t)9, buff, (size_t)buff_len);
        
        freeReplyObject(reply);
    }
}

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
    
    redisReply *reply;
    for(;;) {
        reply = (redisReply*)redisCommand(_db, "RPOP %s", port);
        if(reply->type!=REDIS_REPLY_STRING) {
            break;
        }
        _drvManer->recvedData(h, reply->str, reply->len, nullptr);
    }
    freeReplyObject(reply);
}

//tick all data
void tick(unsigned long long timer) {
    if(!_isOpen) {
        return;
    }

    redisReply *reply;
    
    //read DI
    for(;;) {
        reply = (redisReply*)redisCommand(_db, "RPOP %s", "*DIO2");
        if(reply->type!=REDIS_REPLY_STRING) {
            break;
        }
        _drvManer->recvedDigital(_dio2, strcmp(reply->str, "1")==0);
    }
    freeReplyObject(reply);
    reply = nullptr;

    //read AD
    for(;;) {
        reply = (redisReply*)redisCommand(_db, "RPOP %s", "*AD1");
        if(reply->type!=REDIS_REPLY_STRING) {
            break;
        }
        _drvManer->recvedAnalog(_ad1, atoi(reply->str));
    }
    freeReplyObject(reply);
    reply = nullptr;


    //read Serial1
    recv_data(1);
    
    //read COM1
    recv_data(2);

}

//flush io data
void flush_data(int tag) {
    redisReply* reply;
    if(tag==1) {//Serial1
        reply = (redisReply*)redisCommand(_db,"LTRIM %s 1 0", "*Serial1_OUT");
        freeReplyObject(reply);
        reply = (redisReply*)redisCommand(_db,"LTRIM %s 1 0", "*Serial1_IN");
        freeReplyObject(reply);
    }
    else if(tag==2) {//COM1
        reply = (redisReply*)redisCommand(_db,"LTRIM %s 1 0", "*COM1_OUT");
        freeReplyObject(reply);
        reply = (redisReply*)redisCommand(_db,"LTRIM %s 1 0", "*COM1_IN");
        freeReplyObject(reply);
    }
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

}

void card_close(int tag) {
    _isOpen = false;
}

void e_initial(DrvManer* drvManer) {
    _isOpen = false;
    _db = redisConnectUnix(REDIS_UNIX_SOCKET);
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
    _drvManer = drvManer;
    _drvManer->registerCard("VirtualDevice", 0, card_create, card_open, card_close, tick);
}

void e_release() {
    if(_db!=NULL) {
        redisFree(_db);
    }
}