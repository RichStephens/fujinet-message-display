#include <fujinet-fuji.h>
#include <fujinet-network.h>
#ifndef _CMOC_VERSION_
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#endif /* _CMOC_VERSION_ */

#include "message.h"

char msg[512], old_msg[512];
long last_timestamp = 0;

const char devicespec[] = "N:https://display.irata.online/get";
const char timestampspec[] = "N:https://display.irata.online/timestamp";

bool new_message(char *msg)
{
    char timestamp_str[32];
    long current_timestamp;
#if defined(_CMOC_VERSION_) || defined(__MSDOS__)
    static bool primed = false;
#endif

    // Fetch the current timestamp
    if (network_open(timestampspec, 4, 0) != FN_ERR_OK)
    {
        return false;
    }

    memset(timestamp_str, 0, 32);
    network_read(timestampspec, timestamp_str, 32);
    network_close(timestampspec);

    // Convert timestamp string to long
    current_timestamp = atol(timestamp_str);

#if defined(_CMOC_VERSION_) || defined(__MSDOS__)
    // On the first successful poll, adopt the server's current timestamp as
    // the baseline. Whatever message is already on the server was not
    // received this session, so it must not be displayed at startup.
    if (!primed)
    {
        primed = true;
        last_timestamp = current_timestamp;
        return false;
    }
#endif

    // Check if timestamp is newer than last successful message
    if (current_timestamp <= last_timestamp)
    {
        return false;
    }

    // Fetch the message
    if (network_open(devicespec, 4, 0) != FN_ERR_OK)
    {
        return false;
    }

    memset(msg, 0, 512);
    network_read(devicespec, msg, 512);
    network_close(devicespec);

    // Update last timestamp
    last_timestamp = current_timestamp;

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
