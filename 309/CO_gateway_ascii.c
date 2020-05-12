/*
 * Access from other networks - ASCII mapping (CiA 309-3 DS v2.1.0)
 *
 * @file        CO_gateway_ascii.c
 * @ingroup     CO_CANopen_309_3
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "309/CO_gateway_ascii.h"


/******************************************************************************/
CO_ReturnError_t CO_GTWA_init(CO_GTWA_t* gtwa,
                              CO_SDOclient_t* SDO_C,
                              uint16_t SDOtimeoutTimeDefault,
                              bool_t SDOblockTransferEnableDefault,
                              CO_NMT_t *NMT)
{
    /* verify arguments */
    if (gtwa == NULL
        || SDO_C == NULL || SDOtimeoutTimeDefault == 0
        || NMT == NULL
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* initialize variables */
    gtwa->readCallback = NULL;
    gtwa->readCallbackObject = NULL;
    gtwa->SDO_C = SDO_C;
    gtwa->SDOtimeoutTime = SDOtimeoutTimeDefault;
    gtwa->SDOblockTransferEnable = SDOblockTransferEnableDefault;
    gtwa->NMT = NMT;
    gtwa->net_default = -1;
    gtwa->node_default = -1;
    gtwa->state = CO_GTWA_ST_IDLE;
    gtwa->timeDifference_us_cumulative = 0;
    gtwa->respBufOffset = 0;
    gtwa->respBufCount = 0;
    gtwa->respHold = false;

    CO_fifo_init(&gtwa->commFifo,
                 &gtwa->commBuf[0],
                 CO_CONFIG_GTWA_COMM_BUF_SIZE + 1);

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_GTWA_initRead(CO_GTWA_t* gtwa,
                      size_t (*readCallback)(void *object,
                                             const char *buf,
                                             size_t count),
                      void *readCallbackObject)
{
    if (gtwa != NULL) {
        gtwa->readCallback = readCallback;
        gtwa->readCallbackObject = readCallbackObject;
    }
}


/*******************************************************************************
 * HELPER FUNCTIONS
 ******************************************************************************/
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
/* help string */
static const char *CO_GTWA_helpString =
"Command strings start with '\"[\"<sequence>\"]\"' followed by:\n" \
"[[<net>] <node>] r[ead] <index> <subindex> [<datatype>]        # SDO upload.\n" \
"[[<net>] <node>] w[rite] <index> <subindex> <datatype> <value> # SDO download.\n" \
"\n" \
"[[<net>] <node>] start                   # NMT Start node.\n" \
"[[<net>] <node>] stop                    # NMT Stop node.\n" \
"[[<net>] <node>] preop[erational]        # NMT Set node to pre-operational.\n" \
"[[<net>] <node>] reset node              # NMT Reset node.\n" \
"[[<net>] <node>] reset comm[unication]   # NMT Reset communication.\n" \
"\n" \
"[<net>] set network <value>              # Set default net.\n" \
"[<net>] set node <value>                 # Set default node.\n" \
"[<net>] set sdo_timeout <value>          # Configure SDO time-out.\n" \
"[<net>] set sdo_block <value>            # Enable/disable SDO block transfer.\n" \
"\n" \
"help                                     # Print this help.\n" \
"\n" \
"Datatypes:\n" \
"b                  # Boolean.\n" \
"u8, u16, u32, u64  # Unsigned integers.\n" \
"i8, i16, i32, i64  # Signed integers.\n" \
"r32, r64           # Real numbers.\n" \
"t, td              # Time of day, time difference.\n" \
"vs                 # Visible string (between double quotes if multi-word).\n" \
"os, us, d          # Octet string, unicode string, domain (mime-base64\n" \
"                   # (RFC2045) based, one line).\n" \
"hex                # Hexagonal data, optionally space separated, non-standard.\n" \
"\n" \
"Response:\n" \
"\"[\"<sequence>\"]\" OK | <value> |\n" \
"                 ERROR: <SDO-abort-code> | ERROR: <internal-error-code>\n" \
"\n" \
"Every command must be terminated with <CR><LF> ('\\r\\n'). characters. Same is\n" \
"response. String is not null terminated, <CR> is optional in command.\n" \
"\n" \
"Comments started with '#' are ignored. They may be on the beginning of the\n" \
"line or after the command string.\n" \
"\n" \
"'sdo_timeout' is in milliseconds, 500 by default. Block transfer is disabled\n" \
"by default.\n" \
"\n" \
"If '<net>' or '<node>' is not specified within commands, then value defined by\n" \
"'set network' or 'set node' command is used.\r\n";
#endif


/* Get uint32 number from token, verify limits and set *err if necessary */
static inline uint32_t getU32(char *token, uint32_t min,
                              uint32_t max, bool_t *err)
{
    char *sRet;
    uint32_t num = strtoul(token, &sRet, 0);

    if (sRet != strchr(token, '\0') || num < min || num > max) {
        *err = true;
    }

    return num;
}

/* Verify net and node, return true on error */
static bool_t checkNetNode(CO_GTWA_t *gtwa,
                           int32_t net, int16_t node, uint8_t NodeMin,
                           CO_GTWA_respErrorCode_t *errCode)
{
    bool_t e = false;
    CO_GTWA_respErrorCode_t eCode;

    if (node == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNodeSet;
        e = true;
    }
    else if (node < NodeMin || node > 127) {
        eCode = CO_GTWA_respErrorUnsupportedNode;
        e = true;
    }
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_MULTI_NET
    else if (net == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNetSet;
        e = true;
    }
    /* not implemented */
    else if (net < CO_CONFIG_GTW_NET_MIN || net > CO_CONFIG_GTW_NET_MAX) {
        eCode = CO_GTWA_respErrorUnsupportedNet;
        e = true;
    }
#endif
    else {
        gtwa->net = (uint16_t)net;
        gtwa->node = (uint8_t)node;
    }
    if (e) {
        *errCode = eCode;
    }
    return e;
}

/* Verify net, return true on error */
static bool_t checkNet(CO_GTWA_t *gtwa, int32_t net,
                       CO_GTWA_respErrorCode_t *errCode)
{
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_MULTI_NET
    bool_t e = false;
    CO_GTWA_respErrorCode_t eCode;

    if (net == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNetSet;
        e = true;
    }
    /* not implemented */
    else if (net < CO_CONFIG_GTW_NET_MIN || net > CO_CONFIG_GTW_NET_MAX) {
        eCode = CO_GTWA_respErrorUnsupportedNet;
        e = true;
    }
    else {
        gtwa->net = (uint16_t)net;
    }
    if (e) {
        *errCode = eCode;
    }
    return e;
#else
    #define CO_CONFIG_GTW_NET_MIN 0
    #define CO_CONFIG_GTW_NET_MAX 0xFFFF
    gtwa->net = (uint16_t)net;
    return false;
#endif
}

/* data types for SDO read or write */
static const CO_GTWA_dataType_t dataTypes[] = {
    {"hex", 0, CO_fifo_readHex2a, CO_fifo_cpyTok2Hex},  /* hex, non-standard */
    {"b",   1, CO_fifo_readU82a,  CO_fifo_cpyTok2U8},   /* BOOLEAN */
    {"u8",  1, CO_fifo_readU82a,  CO_fifo_cpyTok2U8},   /* UNSIGNED8 */
    {"u16", 2, CO_fifo_readU162a, CO_fifo_cpyTok2U16},  /* UNSIGNED16 */
    {"u32", 4, CO_fifo_readU322a, CO_fifo_cpyTok2U32},  /* UNSIGNED32 */
    {"u64", 8, CO_fifo_readU642a, CO_fifo_cpyTok2U64},  /* UNSIGNED64 */
    {"i8",  1, CO_fifo_readI82a,  CO_fifo_cpyTok2I8},   /* INTEGER8 */
    {"i16", 2, CO_fifo_readI162a, CO_fifo_cpyTok2I16},  /* INTEGER16 */
    {"i32", 4, CO_fifo_readI322a, CO_fifo_cpyTok2I32},  /* INTEGER32 */
    {"i64", 8, CO_fifo_readI642a, CO_fifo_cpyTok2I64},  /* INTEGER64 */
    {"r32", 4, CO_fifo_readR322a, CO_fifo_cpyTok2R32},  /* REAL32 */
    {"r64", 8, CO_fifo_readR642a, CO_fifo_cpyTok2R64},  /* REAL64 */
//    {"t",   0, CO_GWA_dtpHex, CO_DWA_dtsHex},         /* TIME_OF_DAY */
//    {"td",  0, CO_GWA_dtpHex, CO_DWA_dtsHex},         /* TIME_DIFFERENCE */
    {"vs",  0, CO_fifo_readVs2a,  CO_fifo_cpyTok2Vs}//,
//    {"os",  0, CO_GWA_dtpHex, CO_DWA_dtsHex}, /* ochar_t (OCTET_STRING) (mime-base64 (RFC2045) should be used here) */
//    {"us",  0, CO_GWA_dtpHex, CO_DWA_dtsHex}, /*  (UNICODE_STRING) (mime-base64 (RFC2045) should be used here) */
//    {"d",   0, CO_GWA_dtpHex, CO_DWA_dtsHex}  /* domain_t (DOMAIN) (mime-base64 (RFC2045) should be used here) */
};


/* get data type from token */
static const CO_GTWA_dataType_t *CO_GTWA_getDataType(char *token, bool_t *err) {
    if (token != NULL && *err == false) {
        int i;
        int len = sizeof(dataTypes) / sizeof(CO_GTWA_dataType_t);

        for (i = 0; i < len; i++) {
            const CO_GTWA_dataType_t *dt = &dataTypes[i];
            if (strcmp(token, dt->syntax) == 0) {
                return dt;
            }
        }
    }

    *err = true;
    return NULL;
}


/* transfer response buffer and verify if all bytes was read */
static void respBufTransfer(CO_GTWA_t *gtwa) {
    if (gtwa->readCallback == NULL) {
        /* no callback registered, just purge the response */
        gtwa->respBufOffset = 0;
        gtwa->respBufCount = 0;
        gtwa->respHold = false;
    }
    else {
        /* transfer response to the application */
        size_t countRead =
        gtwa->readCallback(gtwa->readCallbackObject,
                           (const char *)&gtwa->respBuf[gtwa->respBufOffset],
                           gtwa->respBufCount);

        if (countRead < gtwa->respBufCount) {
            gtwa->respBufOffset += countRead;
            gtwa->respBufCount -= countRead;
            gtwa->respHold = true;
        }
        else {
            gtwa->respBufOffset = 0;
            gtwa->respBufCount = 0;
            gtwa->respHold = false;
        }
    }
}


#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC
#ifndef CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS
#define CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS
typedef struct {
    const uint32_t code;
    const char* desc;
} errorDescs_t;

static const errorDescs_t errorDescs[] = {
    {100, "Request not supported."},
    {101, "Syntax error."},
    {102, "Request not processed due to internal state."},
    {103, "Time-out (where applicable)."},
    {104, "No default net set."},
    {105, "No default node set."},
    {106, "Unsupported net."},
    {107, "Unsupported node."},
    {200, "Lost guarding message."},
    {201, "Lost connection."},
    {202, "Heartbeat started."},
    {203, "Heartbeat lost."},
    {204, "Wrong NMT state."},
    {205, "Boot-up."},
    {300, "Error passive."},
    {301, "Bus off."},
    {303, "CAN buffer overflow."},
    {304, "CAN init."},
    {305, "CAN active (at init or start-up)."},
    {400, "PDO already used."},
    {401, "PDO length exceeded."},
    {501, "LSS implementation- / manufacturer-specific error."},
    {502, "LSS node-ID not supported."},
    {503, "LSS bit-rate not supported."},
    {504, "LSS parameter storing failed."},
    {505, "LSS command failed because of media error."},
    {600, "Running out of memory."}
};
static const errorDescs_t errorDescsSDO[] = {
    {0x00000000, "No abort."},
    {0x05030000, "Toggle bit not altered."},
    {0x05040000, "SDO protocol timed out."},
    {0x05040001, "Command specifier not valid or unknown."},
    {0x05040002, "Invalid block size in block mode."},
    {0x05040003, "Invalid sequence number in block mode."},
    {0x05040004, "CRC error (block mode only)."},
    {0x05040005, "Out of memory."},
    {0x06010000, "Unsupported access to an object."},
    {0x06010001, "Attempt to read a write only object."},
    {0x06010002, "Attempt to write a read only object."},
    {0x06020000, "Object does not exist."},
    {0x06040041, "Object cannot be mapped to the PDO."},
    {0x06040042, "Number and length of object to be mapped exceeds PDO length."},
    {0x06040043, "General parameter incompatibility reasons."},
    {0x06040047, "General internal incompatibility in device."},
    {0x06060000, "Access failed due to hardware error."},
    {0x06070010, "Data type does not match, length of service parameter does not match."},
    {0x06070012, "Data type does not match, length of service parameter too high."},
    {0x06070013, "Data type does not match, length of service parameter too short."},
    {0x06090011, "Sub index does not exist."},
    {0x06090030, "Invalid value for parameter (download only)."},
    {0x06090031, "Value range of parameter written too high."},
    {0x06090032, "Value range of parameter written too low."},
    {0x06090036, "Maximum value is less than minimum value."},
    {0x060A0023, "Resource not available: SDO connection."},
    {0x08000000, "General error."},
    {0x08000020, "Data cannot be transferred or stored to application."},
    {0x08000021, "Data cannot be transferred or stored to application because of local control."},
    {0x08000022, "Data cannot be transferred or stored to application because of present device state."},
    {0x08000023, "Object dictionary not present or dynamic generation fails."},
    {0x08000024, "No data available."}
};
#endif /* CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS */

static void responseWithError(CO_GTWA_t *gtwa,
                              CO_GTWA_respErrorCode_t respErrorCode)
{
    int i;
    int len = sizeof(errorDescs) / sizeof(errorDescs_t);
    const char *desc = "-";

    for (i = 0; i < len; i++) {
        const errorDescs_t *ed = &errorDescs[i];
        if(ed->code == respErrorCode) {
            desc = ed->desc;
        }
    }

    gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] ERROR: %d #%s\r\n",
                                 gtwa->sequence, respErrorCode, desc);
    respBufTransfer(gtwa);
}

static void responseWithErrorSDO(CO_GTWA_t *gtwa,
                                 CO_SDO_abortCode_t abortCode)
{
    int i;
    int len = sizeof(errorDescs) / sizeof(errorDescs_t);
    const char *desc = "-";

    for (i = 0; i < len; i++) {
        const errorDescs_t *ed = &errorDescsSDO[i];
        if(ed->code == abortCode) {
            desc = ed->desc;
        }
    }

    gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] ERROR: 0x%08X #%s\r\n",
                                 gtwa->sequence, abortCode, desc);
    respBufTransfer(gtwa);
}

#else /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC */
static inline void responseWithError(CO_GTWA_t *gtwa,
                                     CO_GTWA_respErrorCode_t respErrorCode)
{
    gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] ERROR: %d\r\n",
                                 gtwa->sequence, respErrorCode);
    respBufTransfer(gtwa);
}

static inline void responseWithErrorSDO(CO_GTWA_t *gtwa,
                                        CO_SDO_abortCode_t abortCode)
{
    gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] ERROR: 0x%08X\r\n",
                                 gtwa->sequence, abortCode);
    respBufTransfer(gtwa);
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC */


static inline void responseWithOK(CO_GTWA_t *gtwa) {
    gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] OK\r\n", gtwa->sequence);
    respBufTransfer(gtwa);
}


/*******************************************************************************
 * PROCESS FUNCTION
 ******************************************************************************/
void CO_GTWA_process(CO_GTWA_t *gtwa,
                     bool_t enable,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
    bool_t err = false; /* syntax or other error, true or false, I/O variable */
    char closed; /* indication of command delimiter, I/O variable */
    CO_GTWA_respErrorCode_t respErrorCode = CO_GTWA_respErrorNone;

    if (gtwa == NULL) {
        return;
    }

    if (!enable) {
        gtwa->state = CO_GTWA_ST_IDLE;
        CO_fifo_reset(&gtwa->commFifo);
        return;
    }

    /* If there is some more output data for application, read them first.
     * Hold on this state, if necessary. */
    if (gtwa->respHold) {
        timeDifference_us += gtwa->timeDifference_us_cumulative;

        respBufTransfer(gtwa);
        if (gtwa->respHold) {
            gtwa->timeDifference_us_cumulative = timeDifference_us;
            return;
        }
        else {
            gtwa->timeDifference_us_cumulative = 0;
        }
    }

    /***************************************************************************
    * COMMAND PARSER
    ***************************************************************************/
    /* if idle, search for new command, skip comments or empty lines */
    while (gtwa->state == CO_GTWA_ST_IDLE
           && CO_fifo_CommSearch(&gtwa->commFifo, false)
    ) {
        char token[20]; /* must be the size of tokenSize */
        const size_t tokenSize = 20;
        size_t n;
        uint32_t ui[3];
        int i;
        int32_t net = gtwa->net_default;
        int16_t node = gtwa->node_default;


        /* parse mandatory token '"["<sequence>"]"' */
        closed = -1;
        n = CO_fifo_readToken(&gtwa->commFifo, token, tokenSize, &closed, &err);
        /* Break if error in token or token was found, but closed with
         * command delimiter. */
        if (err || (n > 0 && closed != 0)) {
            err = true;
            break;
        }
        /* If empty line or just comment, continue with next command */
        else if (n == 0 && closed != 0) {
            continue;
        }
        if (token[0] != '[' || token[strlen(token)-1] != ']') {
            err = true;
            break;
        }
        token[strlen(token)-1] = '\0';
        gtwa->sequence = getU32(token+1, 0, 0xFFFFFFFF, &err);
        if (err) break;


        /* parse optional tokens '[[<net>] <node>]', both numerical. Then
         * follows mandatory token <command>, which is not numerical. */
        for (i = 0; i < 3; i++) {
            closed = -1;
            n = CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
            if (err || n == 0) {
                /* empty token, break on error */
                err = true;
                break;
            } else if (isdigit(token[0]) == 0) {
                /* <command> found */
                break;
            } else if (closed != 0) {
                /* numerical value must not be closed */
                err = true;
                break;
            }

            ui[i] = getU32(token, 0, 0xFFFFFFFF, &err);
            if (err) break;
        }
        if (err) break;

        switch(i) {
        case 0: /* only <command> (pointed by token) */
            break;
        case 1: /* <node> and <command> tokens */
            if (ui[0] > 127) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNode;
            }
            else {
                node = (int16_t) ui[0];
            }
            break;
        case 2: /* <net>, <node> and <command> tokens */
            if (ui[0] > 0xFFFF) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNet;
            }
            else if (ui[1] > 127) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNode;
            }
            else {
                net = (int32_t) ui[0];
                node = (int16_t) ui[1];
            }
            break;
        case 3: /* <command> token contains digit */
            err = true;
            break;
        }
        if (err) break;


        /* Upload SDO command - 'r[ead] <index> <subindex> <datatype>' */
        if (strcmp(token, "r") == 0 || strcmp(token, "read") == 0) {
            uint16_t idx;
            uint8_t subidx;
            CO_SDOclient_return_t SDO_ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 1, &respErrorCode);

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* index */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, token, tokenSize, &closed, &err);
            idx = (uint16_t)getU32(token, 0, 0xFFFF, &err);
            if (err) break;

            /* subindex */
            closed = -1;
            n = CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
            subidx = (uint8_t)getU32(token, 0, 0xFF, &err);
            if (err || n == 0) {
                err = true;
                break;
            }

            /* optional data type */
            if (closed == 0) {
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
                gtwa->SDOdataType = CO_GTWA_getDataType(token, &err);
                if (err) break;
            }
            else {
                gtwa->SDOdataType = &dataTypes[0]; /* use generic data type */
            }

            /* setup client */
            SDO_ret = CO_SDOclient_setup(gtwa->SDO_C, 0, 0, gtwa->node);
            if (SDO_ret != CO_SDOcli_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* initiate upload */
            SDO_ret = CO_SDOclientUploadInitiate(gtwa->SDO_C, idx, subidx,
                                                 gtwa->SDOtimeoutTime,
                                                 gtwa->SDOblockTransferEnable);
            if (SDO_ret != CO_SDOcli_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* indicate that gateway response didn't start yet */
            gtwa->SDOdataCopyStatus = false;
            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_READ;
        }

        /* Download SDO comm. - w[rite] <index> <subindex> <datatype> <value> */
        else if (strcmp(token, "w") == 0 || strcmp(token, "write") == 0) {
            uint16_t idx;
            uint8_t subidx;
            uint8_t status;
            CO_SDOclient_return_t SDO_ret;
            size_t size;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 1, &respErrorCode);

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* index */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, token, tokenSize, &closed, &err);
            idx = (uint16_t)getU32(token, 0, 0xFFFF, &err);
            if (err) break;

            /* subindex */
            closed = 0;
            n = CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
            subidx = (uint8_t)getU32(token, 0, 0xFF, &err);
            if (err) break;

            /* data type */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                              &closed, &err);
            gtwa->SDOdataType = CO_GTWA_getDataType(token, &err);
            if (err) break;

            /* setup client */
            SDO_ret = CO_SDOclient_setup(gtwa->SDO_C, 0, 0, gtwa->node);
            if (SDO_ret != CO_SDOcli_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* initiate download */
            SDO_ret =
            CO_SDOclientDownloadInitiate(gtwa->SDO_C, idx, subidx,
                                         gtwa->SDOdataType->length,
                                         gtwa->SDOtimeoutTime,
                                         gtwa->SDOblockTransferEnable);
            if (SDO_ret != CO_SDOcli_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* copy data from comm to the SDO buffer, according to data type */
            size = gtwa->SDOdataType->dataTypeScan(&gtwa->SDO_C->bufFifo,
                                                   &gtwa->commFifo,
                                                   &status);
            /* set to true, if command delimiter was found */
            closed = (status & 0x01) == 1;
            /* set to true, if data are copied only partially */
            gtwa->SDOdataCopyStatus = (status & 0x02) != 0;

            /* is syntax error in command or size is zero or not the last token
             * in command */
            if ((status & 0xF0) != 0 || size == 0
                || (gtwa->SDOdataCopyStatus == false && closed != 1)
            ) {
                err = true;
                break;
            }

            /* if data size was not known before and is known now, update SDO */
            if (gtwa->SDOdataType->length == 0 && !gtwa->SDOdataCopyStatus) {
                CO_SDOclientDownloadInitiateSize(gtwa->SDO_C, size);
            }

            /* continue with state machine */
            gtwa->stateTimeoutTmr = 0;
            gtwa->state = CO_GTWA_ST_WRITE;
        }

        /* NMT start node */
        else if (strcmp(token, "start") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_OPERATIONAL;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT stop node */
        else if (strcmp(token, "stop") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_STOPPED;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT Set node to pre-operational  */
        else if (strcmp(token, "preop") == 0 ||
                 strcmp(token, "preoperational") == 0
        ) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_PRE_OPERATIONAL;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT reset (node or communication) */
        else if (strcmp(token, "reset") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* command 2 */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, token, tokenSize, &closed, &err);
            if (err) break;

            if (strcmp(token, "node") == 0) {
                command2 = CO_NMT_RESET_NODE;
            } else if (strcmp(token, "comm") == 0 ||
                       strcmp(token, "communication") == 0
            ) {
                command2 = CO_NMT_RESET_COMMUNICATION;
            } else {
                err = true;
                break;
            }

            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* set command - multiple sub commands */
        else if (strcmp(token, "set") == 0) {
            if (closed != 0) {
                err = true;
                break;
            }

            /* command 2 */
            closed = -1;
            CO_fifo_readToken(&gtwa->commFifo, token, tokenSize, &closed, &err);
            if (err) break;

            if (strcmp(token, "network") == 0) {
                uint16_t value;

                if (closed != 0) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
                value = (uint16_t)getU32(token, CO_CONFIG_GTW_NET_MIN,
                                         CO_CONFIG_GTW_NET_MAX, &err);
                if (err) break;

                gtwa->net_default = value;
                responseWithOK(gtwa);
            }
            else if (strcmp(token, "node") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint8_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
                value = (uint8_t)getU32(token, 1, 127, &err);
                if (err) break;

                gtwa->node_default = value;
                responseWithOK(gtwa);
            }
            else if (strcmp(token, "sdo_timeout") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint16_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
                value = (uint16_t)getU32(token, 1, 0xFFFF, &err);
                if (err) break;

                gtwa->SDOtimeoutTime = value;
                responseWithOK(gtwa);
            }
            else if (strcmp(token, "sdo_block") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint16_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, token, tokenSize,
                                  &closed, &err);
                value = (uint16_t)getU32(token, 0, 1, &err);
                if (err) break;

                gtwa->SDOblockTransferEnable = value==1 ? true : false;
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorReqNotSupported;
                err = true;
                break;
            }
        }

        /* TODO LSS */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
        /* Print help */
        else if (strcmp(token, "help") == 0) {
            gtwa->helpStringOffset = 0;
            gtwa->state = CO_GTWA_ST_HELP;
        }
#endif

        /* Unrecognized command */
        else {
            respErrorCode = CO_GTWA_respErrorReqNotSupported;
            err = true;
            break;
        }
    } /* while CO_GTWA_ST_IDLE && CO_fifo_CommSearch */



    /***************************************************************************
    * STATE MACHINE
    ***************************************************************************/
    /* If error, generate error response */
    if (err) {
        if (respErrorCode == CO_GTWA_respErrorNone) {
            respErrorCode = CO_GTWA_respErrorSyntax;
        }
        responseWithError(gtwa, respErrorCode);

        /* delete command, if it was only partially read */
        if(closed == 0) {
            CO_fifo_CommSearch(&gtwa->commFifo, true);
        }
        gtwa->state = CO_GTWA_ST_IDLE;
    }

    /* SDO upload state */
    else if (gtwa->state == CO_GTWA_ST_READ) {
        CO_SDO_abortCode_t abortCode;
        size_t sizeTransferred;
        CO_SDOclient_return_t ret;

        ret = CO_SDOclientUpload(gtwa->SDO_C,
                                 timeDifference_us,
                                 &abortCode,
                                 NULL,
                                 &sizeTransferred,
                                 timerNext_us);

        if (ret < 0) {
            responseWithErrorSDO(gtwa, abortCode);
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        /* Response data must be read, partially or whole */
        else if (ret == CO_SDOcli_uploadDataBufferFull
                 || ret == CO_SDOcli_ok_communicationEnd
        ) {
            size_t fifoRemain;

            /* write response head first */
            if (!gtwa->SDOdataCopyStatus) {
                gtwa->respBufCount = sprintf(gtwa->respBuf, "[%d] ",
                                             gtwa->sequence);
                gtwa->SDOdataCopyStatus = true;
            }

            /* Empty SDO fifo buffer in multiple cycles. Repeat until
             * application runs out of space (respHold) or fifo empty. */
            do {
                /* read SDO fifo (partially) and print specific data type as
                 * ascii into intermediate respBuf */
                gtwa->respBufCount += gtwa->SDOdataType->dataTypePrint(
                    &gtwa->SDO_C->bufFifo,
                    &gtwa->respBuf[gtwa->respBufCount],
                    CO_GTWA_RESP_BUF_SIZE - 2 - gtwa->respBufCount,
                    ret == CO_SDOcli_ok_communicationEnd);
                fifoRemain = CO_fifo_getOccupied(&gtwa->SDO_C->bufFifo);

                /* end of communication, print newline and enter idle state */
                if (ret == CO_SDOcli_ok_communicationEnd && fifoRemain == 0) {
                    gtwa->respBufCount +=
                        sprintf(&gtwa->respBuf[gtwa->respBufCount], "\r\n");
                    gtwa->state = CO_GTWA_ST_IDLE;
                }

                /* transfer response to the application */
                respBufTransfer(gtwa);
            } while (gtwa->respHold == false && fifoRemain > 0);
        }
    }

    /* SDO download state */
    else if (gtwa->state == CO_GTWA_ST_WRITE) {
        CO_SDO_abortCode_t abortCode;
        size_t sizeTransferred;
        bool_t abort = false;
        bool_t hold = false;
        CO_SDOclient_return_t ret;
        int loop = 0;

        /* copy data to the SDO buffer if more data available */
        if (gtwa->SDOdataCopyStatus) {
            uint8_t status;
            gtwa->SDOdataType->dataTypeScan(&gtwa->SDO_C->bufFifo,
                                            &gtwa->commFifo,
                                            &status);
            /* set to true, if command delimiter was found */
            closed = (status & 0x01) == 1;
            /* set to true, if data are copied only partially */
            gtwa->SDOdataCopyStatus = (status & 0x02) != 0;

            /* is syntax error in command or not the last token in command */
            if ((status & 0xF0) != 0
                || (gtwa->SDOdataCopyStatus == false && closed != 1)
            ) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                abort = true; /* abort SDO communication */
            }
        }
        /* If not all data were transferred, make sure, there is enough data in
         * SDO buffer, to continue communication. Otherwise wait and check for
         * timeout */
        if (gtwa->SDOdataCopyStatus
            && CO_fifo_getOccupied(&gtwa->SDO_C->bufFifo) <
               (CO_CONFIG_GTW_BLOCK_DL_LOOP * 7)
        ) {
            if (gtwa->stateTimeoutTmr > CO_GTWA_STATE_TIMEOUT_TIME_US) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                abort = true;
            }
            else {
                gtwa->stateTimeoutTmr += timeDifference_us;
                hold = true;
            }
        }
        if (!hold) {
            /* if OS has CANtx queue, speedup block transfer */
            do {
                ret = CO_SDOclientDownload(gtwa->SDO_C,
                                           timeDifference_us,
                                           abort,
                                           &abortCode,
                                           &sizeTransferred,
                                           timerNext_us);
                if (++loop >= CO_CONFIG_GTW_BLOCK_DL_LOOP) {
                    break;
                }
            } while (ret == CO_SDOcli_blockDownldInProgress);

            /* send response in case of error or finish */
            if (ret < 0) {
                responseWithErrorSDO(gtwa, abortCode);
                gtwa->state = CO_GTWA_ST_IDLE;
            }
            else if (ret == CO_SDOcli_ok_communicationEnd) {
                responseWithOK(gtwa);
                gtwa->state = CO_GTWA_ST_IDLE;
            }
        }
    }

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
    /* Print help string (in multiple segments if necessary) */
    else if (gtwa->state == CO_GTWA_ST_HELP) {
        size_t lenBuf = CO_GTWA_RESP_BUF_SIZE;
        size_t lenHelp = strlen(CO_GTWA_helpString);

        do {
            size_t lenHelpRemain = lenHelp - gtwa->helpStringOffset;
            size_t lenCopied = lenBuf < lenHelpRemain ? lenBuf : lenHelpRemain;

            memcpy(gtwa->respBuf,
                    &CO_GTWA_helpString[gtwa->helpStringOffset],
                    lenCopied);

            gtwa->respBufCount = lenCopied;
            gtwa->helpStringOffset += lenCopied;
            respBufTransfer(gtwa);

            if (gtwa->helpStringOffset == lenHelp) {
                gtwa->state = CO_GTWA_ST_IDLE;
                break;
            }
        } while (gtwa->respHold == false);
    }
#endif

    /* illegal state */
    else if (gtwa->state != CO_GTWA_ST_IDLE) {
        respErrorCode = CO_GTWA_respErrorInternalState;
        responseWithError(gtwa, respErrorCode);
        gtwa->state = CO_GTWA_ST_IDLE;
    }

}
