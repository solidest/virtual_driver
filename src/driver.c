
#include <iostream>
#include <fstream>
#include <string>
#include "driver.h"
#include "./concurrentqueue.h"
#include "json.hpp"

using namespace moodycamel;
using namespace std;
using namespace nlohmann;

static DrvManer* _drvManer;
static bool _isOpen;

InterfaceHandle _dio1_out;
InterfaceHandle _dio2_in;
InterfaceHandle _ad1_in;
InterfaceHandle _da1_out;
InterfaceHandle _serial1;
InterfaceHandle _com1;

typedef struct DataInfo {
    string* data;
    DrvObject* option;
} DataInfo;

static ConcurrentQueue<bool> digitalQueue;
static ConcurrentQueue<int> analogQueue;
static ConcurrentQueue<DataInfo> serialQueue;
static ConcurrentQueue<DataInfo> comQueue;

//send digital out
void send_digital(int tag, bool value) {

    if(tag==1) {//DO DIO1
        digitalQueue.enqueue(value);
    }
    else if(tag==2) {//DI DIO2
        _drvManer->logError("can not send out on dio2_in");
        return;
    }
}

//send analog out
void send_analog(int tag, int value) {
    if(tag==1) {//AD1
        _drvManer->logError("can not send out on ad1_in");
    }
    else if(tag==2) {//DA1 send value
        analogQueue.enqueue(value);
    }
}

//send stream data out
void send_data(int tag, const char* buff, unsigned int buff_len, DrvObject* option) {
    DataInfo info;
    info.data = new string(buff, buff_len);
    info.option = cloneDrvObject(option);

    if(tag==1) {//Serial1
        serialQueue.enqueue(info);
    }
    else if(tag==2) {//COM1
        comQueue.enqueue(info);
    }
}

//recv stream data
void recv_data(int tag) {
    if(tag==1) {//Serial1
        DataInfo info;
        while(serialQueue.try_dequeue(info)) {
            _drvManer->recvedData(_serial1, info.data->c_str(), info.data->size(), info.option);
            delete info.data;
            freeDrvObject(info.option);
        }
    }
    else if(tag==2) {//COM1
        DataInfo info;
        while(comQueue.try_dequeue(info)) {
            _drvManer->recvedData(_com1, info.data->c_str(), info.data->size(), info.option);
            delete info.data;
            freeDrvObject(info.option);
        }
    }
}

//tick all data
void tick(unsigned long long timer) {

    if(!_isOpen) {
        return;
    }

    bool b;
    while(digitalQueue.try_dequeue(b)) {
        _drvManer->recvedDigital(_dio2_in, b);
    }

    int i;
    while(analogQueue.try_dequeue(i)) {
        _drvManer->recvedAnalog(_ad1_in, i);
    }

    //read Serial1
    recv_data(1);

    //read COM1
    recv_data(2);
}

//flush io data
void flush_data(int tag) {
    bool b;
    int i;
    DataInfo info;

    while(digitalQueue.try_dequeue(b));
    while(analogQueue.try_dequeue(i));
    while(serialQueue.try_dequeue(info)) {
        delete info.data;
        freeDrvObject(info.option);
    }
    while(comQueue.try_dequeue(info)) {
        delete info.data;
        freeDrvObject(info.option);
    }
}

//create interface of card
void card_create(int tag, const char* config_file) {
    json intrefaces;
    ifstream i(config_file);
    i >> intrefaces;
    for(auto& inf : intrefaces) {
        auto inf_id = inf["interface"].get<string>();
        auto inf_name = inf["name"].get<string>().c_str();
        if(inf_id == "DIO1") {
            _dio1_out = _drvManer->registerDigitalInterface(inf_name, 1, send_digital);
        } else if(inf_id == "DIO2") {
            _dio2_in = _drvManer->registerDigitalInterface(inf_name, 2, nullptr);
        } else if(inf_id=="AD1") {
            _ad1_in = _drvManer->registerAnalogInterface(inf_name, 1, nullptr);
        } else if(inf_id == "DA1") {
            _da1_out = _drvManer->registerAnalogInterface(inf_name, 2, send_analog);
        } else if(inf_id == "Serial1") {
            _serial1 = _drvManer->registerDataInterface(inf_name, 1, send_data, recv_data, flush_data);
        } else if(inf_id == "COM1") {
            _com1 = _drvManer->registerDataInterface(inf_name, 2, send_data, recv_data, flush_data);
        }
    }
}

void card_open(int tag) {
    flush_data(0);
    _isOpen = true;
}

void card_close(int tag) {
    flush_data(0);
    _isOpen = false;
}

void e_initial(DrvManer* drvManer) {
    _isOpen = false;
    _drvManer = drvManer;
    _drvManer->registerCard("VirtualDevice", 0, card_create, card_open, card_close, tick);
}

void e_release() {
    flush_data(0);
}