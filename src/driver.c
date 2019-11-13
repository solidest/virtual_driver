
#include "driver.h"

static DrvManer* _drvManer;

void card_create(int tag, json& interfaces_config) {

}

void card_open(int tag) {

}

void card_close(int tag) {

}

void e_initial(DrvManer* drvManer) {
    _drvManer = drvManer;
    _drvManer->registerCard("VirtualInterface", 0, card_create, card_open, card_close, nullptr);
}

void e_release() {

}