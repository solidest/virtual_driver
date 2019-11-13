/*
** drvdef.h for DrvMan in /home/pi/DrvMan/src
**
** Made by solidest
** Login   <>
**
** Started on  Mon Oct 28 1:24:10 PM 2019 solidest
** Last update Thu Nov 13 11:22:03 AM 2019 solidest
*/

#ifndef _DRVDEF_H_
#define _DRVDEF_H_

#include "../json.hpp"

using namespace std;
using namespace nlohmann;

#define DRV_OK 0
#define DRV_ERROR -100

struct DrvManer;

typedef void* InterfaceHandle;

//in driver for cit
typedef void(*recv_data_callback)(int tag);
typedef int(*counting_callback)(int tag);
typedef void(*exfun_callback)(json& in, json& out);
typedef void(*tick_callback)(unsigned long long timer); 

typedef void(*send_data_callback)(int tag, const char* buff, unsigned int buff_len, json* option);
typedef void(*send_digital_callback)(int tag, bool value);
typedef void(*send_analog_callback)(int tag, int value);
typedef void(*flush_callback)(int tag);

typedef void(*card_create_callback)(int tag, json& configs);
typedef void(*card_open_callback)(int tag);
typedef void(*card_close_callback)(int tag);

//in cit for driver
typedef void(*fdwatch_callback)(int fd, InterfaceHandle interface, int tag);
typedef void(*selectwatch_callback)(counting_callback counting, InterfaceHandle interface, int tag);

typedef void(*recved_data)(InterfaceHandle interface, const char* buff, unsigned int buff_len, json* option);
typedef void(*recved_digital)(InterfaceHandle interface, bool value);
typedef void(*recved_analog)(InterfaceHandle interface, int value);

typedef void(*register_card_callback)(const char* card_name, int tag, card_create_callback create, card_open_callback open, card_close_callback close, tick_callback tick);
typedef InterfaceHandle(*register_datainterface_callback)(const char*  interface_name, int tag, send_data_callback send, recv_data_callback recv, flush_callback flush);
typedef InterfaceHandle(*register_digitalinterface_callback)(const char*  interface_name, int tag, send_digital_callback send);
typedef InterfaceHandle(*register_analoginterface_callback)(const char*  interface_name, int tag, send_analog_callback send);
typedef void(*register_exfun_callback)(InterfaceHandle interface, const char* exfun_name, exfun_callback exfun);

typedef void(*log_error)(const char* err);
typedef void(*log_info)(const char* err);

//for initial
typedef struct DrvManer {
    register_card_callback registerCard;
    register_datainterface_callback registerDataInterface;
    register_digitalinterface_callback registerDigitalInterface;
    register_analoginterface_callback registerAnalogInterface;
    register_exfun_callback registerExfun;
    fdwatch_callback watchFD;
    selectwatch_callback watchSelect;
    recved_data recvedData;
    recved_digital recvedDigital;
    recved_analog recvedAnalog;
    log_error logError;
    log_info logInfo;
} DrvManer;

//API
// typedef void(*e_initial)(DrvManer* drvManer);
// typedef void(*e_release)();

#endif