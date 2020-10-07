#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"

static const OD_entry_t OD_list[] = {
    {0x1000, 0, 0, 0, NULL}
};

const OD_t OD = {
    sizeof(OD_list) / sizeof(OD_list[0]),
    &OD_list[0]
};
