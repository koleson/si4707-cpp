//
// Created by koleson on 12/4/23.
//

#include "si4707_structs.h"


// compares only SAME status parameters of SAME status packets.
// returns true if all status parameters of both packets are the same; false otherwise
// does not compare received DATA or CONF (confidence).
bool equal_SAME_status_packets(const struct Si4707_SAME_Status_Packet* p1, const struct Si4707_SAME_Status_Packet* p2) {
    if (
        p1->EOMDET == p2->EOMDET
        && p1->SOMDET == p2->SOMDET
        && p1->PREDET == p2->PREDET
        && p1->HDRRDY == p2->HDRRDY
        && p1->STATE == p2->STATE
        && p1->MSGLEN == p1->MSGLEN
            ) { return true; }
    return false;
}

