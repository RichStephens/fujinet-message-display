#include <fujinet-fuji.h>
#include <fujinet-network.h>
#ifndef _CMOC_VERSION_
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#endif /* _CMOC_VERSION_ */

#include "message.h"

char msg[512], old_msg[512];

const char devicespec[] = "N:https://display.irata.online/get";

bool new_message(char *msg)
{
    if (network_open(devicespec,4,0) != FN_ERR_OK)
    {
        return false;
    }

    memset(msg,0,512);
    network_read(devicespec,msg,512);

    if (!strcmp(msg,old_msg))
    {
        network_close(devicespec);
        return false;
    }

    memset(old_msg,0,512);
    strcpy(old_msg,msg);
    network_close(devicespec);

    return true;
}

void main()
{
    while(1)
    {
        if (new_message(msg))
            display_message(msg);
        else
            show_qr_code();
    }
}
