//
// Created by koleson on 12/4/23.
//

#ifndef SI4707_CPP_SI4707_STRUCTS_H
#define SI4707_CPP_SI4707_STRUCTS_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pico/stdlib.h"

struct Si4707_Heartbeat {
    // int i, bool si4707_started, uint8_t rssi, uint8_t snr, bool tune_valid
    unsigned int iteration;
    bool si4707_started;
    uint8_t rssi;
    uint8_t snr;
    bool tune_valid;
};

struct Si4707_RSQ_Status {
    // unsigned int CTS: 		1;
    // unsigned int ERR: 		1;
    // unsigned int RSQINT: 	1;
    // unsigned int SAMEINT: 	1;
    // unsigned int ASQINT:	1;
    // unsigned int SNRHINT:	1;
    // unsigned int SNRLINT:	1;
    // unsigned int RSSIHINT:  1;
    // unsigned int RSSILINT:	1;
    // unsigned int AFCRL:		1;
    // unsigned int VALID:		1;
    uint8_t RSSI;
    uint8_t ASNR;
    uint8_t FREQOFF;
};

struct Si4707_SAME_Status_Params {
    // byte 0:  command

    // byte 1:  Clear Buffer (D1), Acknowledge SAMEINT (LSB/D0)
    bool CLRBUF;
    bool INTACK;

    // byte 2:  offset to start reading SAME message
    uint8_t READADDR;
};

struct Si4707_SAME_Status_Packet {
    unsigned int CTS: 		1;
    unsigned int ERR: 		1;
    unsigned int RSQINT: 	1;
    unsigned int SAMEINT: 	1;
    unsigned int ASQINT:	1;
    unsigned int STCINT: 	1;
    unsigned int EOMDET: 	1;
    unsigned int SOMDET: 	1;
    unsigned int PREDET: 	1;
    unsigned int HDRRDY: 	1;
    uint8_t STATE;
    uint8_t MSGLEN;

    // confidence for the 8 bytes in response, 0-3, 3 being most confident
    uint8_t CONF[2];

    // 8 bytes of the SAME header starting at `READADDR` specified in params
    uint8_t DATA[8];
};

struct Si4707_SAME_Status_FullResponse {
    unsigned int CTS: 		1;
    unsigned int ERR: 		1;
    unsigned int RSQINT: 	1;
    unsigned int SAMEINT: 	1;
    unsigned int ASQINT:	1;
    unsigned int STCINT: 	1;
    unsigned int EOMDET: 	1;
    unsigned int SOMDET: 	1;
    unsigned int PREDET: 	1;
    unsigned int HDRRDY: 	1;
    uint8_t STATE;
    uint8_t MSGLEN;

    uint8_t* CONF;
    uint8_t* DATA;
};

struct Si4707_Command_Args {
    uint8_t ARG1;
    uint8_t ARG2;
    uint8_t ARG3;
    uint8_t ARG4;
    uint8_t ARG5;
    uint8_t ARG6;
    uint8_t ARG7;
};

bool equal_status_packets(struct Si4707_SAME_Status_Packet * p1, struct Si4707_SAME_Status_Packet * p2);

#endif //SI4707_CPP_SI4707_STRUCTS_H
