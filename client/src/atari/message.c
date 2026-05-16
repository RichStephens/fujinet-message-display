/**
 * @brief   {brief}
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details
 * @verbose {verbose}
 */

#include <atari.h>
#include <string.h>
#include "display_data.h"

char display_msg[512];

/**
 * @brief convert ASCII char to screen code
 */
char convert_char(char c)
{
    if (c<96)
        return c-32;
    else
        return c;
}

void screen_common(void)
{
    OS.soundr=0;
    OS.rtclok[0] = OS.rtclok[1] = OS.rtclok[2] = 0; // reset clock
    memcpy((void *)0x0600, font, 1024); // copy font into place
    OS.chbas = 0x06; // and use it
    OS.color0 = _gtia_mkcolor(HUE_GREEN,7); // the QR code, lightest green
    OS.color1 = _gtia_mkcolor(HUE_GREEN,5);
    OS.color2 = _gtia_mkcolor(HUE_BLUE,7);
    OS.color3 = _gtia_mkcolor(HUE_MAGENTA,7);
    OS.color4 = _gtia_mkcolor(HUE_GREEN,0);  // the background, darkest green
}

void fade(void)
{
    unsigned char i=0;

    for (i=0;i<8;i++)
    {
        if (OS.color0)
        {
            OS.rtclok[2] = 0;
            OS.color0--;
            while (OS.rtclok[2] < 1);
        }
        if (OS.color1)
        {
            OS.rtclok[2] = 0;
            OS.color1--;
            while (OS.rtclok[2] < 1);
        }
        if (OS.color2)
        {
            OS.rtclok[2] = 0;
            OS.color2--;
            while (OS.rtclok[2] < 1);
        }
        if (OS.color3)
        {
            OS.rtclok[2] = 0;
            OS.color3--;
            while (OS.rtclok[2] < 1);
        }
    }
}

void show_qr_code(void)
{
    screen_common();

    OS.sdlst = (void *)&dlist; // use the waiting display list

    while (OS.rtclok[2] < 120); // and wait a bit.
}

#define MSG_COLS 20
#define MSG_ROWS 25

void display_message(const char *msg)
{
    int row=0, col=0, i=0, wlen, k;
    char c;

    fade();

    screen_common();

    OS.sdlst = (void *)&msg_dlist;
    memset(display_msg,0x00,sizeof(display_msg));

    while (msg[i] != 0x00 && row < MSG_ROWS)
    {
        c = msg[i];

        if (c == 0x0A) // newline
        {
            row++;
            col = 0;
            i++;
            continue;
        }

        if (c == 0x20) // space
        {
            if (col > 0 && col < MSG_COLS)
                col++;
            i++;
            continue;
        }

        // measure the next word
        wlen = 0;
        while (msg[i+wlen] != 0x00 && msg[i+wlen] != 0x20 && msg[i+wlen] != 0x0A)
            wlen++;

        // wrap if the word will not fit on the current line
        if (col > 0 && col + wlen > MSG_COLS)
        {
            row++;
            col = 0;
        }

        for (k=0; k<wlen && row<MSG_ROWS; k++)
        {
            c = msg[i+k];
            if (c < 0x20 || c > 0x7F) // non-printable ASCII? ignore.
                continue;
            if (col >= MSG_COLS)
            {
                row++;
                col = 0;
                if (row >= MSG_ROWS)
                    break;
            }
            display_msg[row*MSG_COLS + col] = convert_char(c);
            col++;
        }
        i += wlen;
    }

    while (OS.rtclok[1] < 2); // and wait a bit.

    fade();
}
