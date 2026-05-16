/**
 * @brief   Message display - CoCo (PMODE 4 graphics, hirestxt text)
 * @license gpl v. 3, see LICENSE for details
 */

#include <cmoc.h>
#include <coco.h>
#include "hirestxt.h"
#include "qrcode-pmode4.h"

#define SCREEN_BUFFER ((byte *)0x0A00)
#define SCREEN_BYTES  6144

#define TEXT_COLS 42
#define TEXT_ROWS 24

#define HEADER  "MESSAGE RECEIVED"
#define MSG_ROW 2

#define QR_TICKS   120
#define HOLD_TICKS 300
#define FADE_TICKS 2
#define FADE_STEP  6

enum { SCREEN_NONE, SCREEN_QR, SCREEN_MSG };

static byte screen_ready = 0;
static byte current_screen = SCREEN_NONE;
static char fade_grid[TEXT_ROWS][TEXT_COLS];

static void wait_ticks(word ticks)
{
    setTimer(0);

    while (getTimer() < ticks)
        ;
}

static void screen_init(void)
{
    struct HiResTextScreenInit init =
        {
            TEXT_COLS,
            writeCharAt_42cols,
            SCREEN_BUFFER,
            TRUE,
            (word *)0x0112,
            0,
            NULL,
            NULL,
        };

    if (screen_ready)
        return;

    initCoCoSupport();
    width(32);
    pmode(4, SCREEN_BUFFER);
    pcls(255);
    screen(1, 1);
    initHiResTextScreen(&init);
    setScreenInverted(TRUE);

    screen_ready = 1;
}

void show_qr_code(void)
{
    screen_init();

    if (current_screen != SCREEN_QR)
    {
        memcpy(SCREEN_BUFFER, qrcode_pmode4_gray, SCREEN_BYTES);
        current_screen = SCREEN_QR;
    }

    wait_ticks(QR_TICKS);
}

void display_message(const char *msg)
{
    byte row, col;
    byte len, start;
    byte any;
    word i;
    int wlen, k;
    char c;

    screen_init();

    memset(fade_grid, ' ', sizeof(fade_grid));

    len = (byte)strlen(HEADER);
    start = (TEXT_COLS - len) / 2;
    for (col = 0; col < len; col++)
        fade_grid[0][start + col] = HEADER[col];

    row = MSG_ROW;
    col = 0;
    i = 0;
    while (msg[i] != '\0' && row < TEXT_ROWS)
    {
        c = msg[i];

        if (c == '\n')
        {
            row++;
            col = 0;
            i++;
            continue;
        }

        if (c == ' ')
        {
            if (col > 0 && col < TEXT_COLS)
                col++;
            i++;
            continue;
        }

        wlen = 0;
        while (msg[i + wlen] != '\0' && msg[i + wlen] != ' ' && msg[i + wlen] != '\n')
            wlen++;

        if (col > 0 && col + wlen > TEXT_COLS)
        {
            row++;
            col = 0;
        }

        for (k = 0; k < wlen && row < TEXT_ROWS; k++)
        {
            c = msg[i + k];
            if (c < 0x20 || c > 0x7E)
                continue;
            if (col >= TEXT_COLS)
            {
                row++;
                col = 0;
                if (row >= TEXT_ROWS)
                    break;
            }
            fade_grid[row][col++] = c;
        }
        i += wlen;
    }

    for (row = 0; row < TEXT_ROWS; row++)
        for (col = 0; col < TEXT_COLS; col++)
            writeCharAt_42cols(col, row, (byte)fade_grid[row][col]);

    current_screen = SCREEN_MSG;

    wait_ticks(HOLD_TICKS);

    do
    {
        any = 0;

        for (row = 0; row < TEXT_ROWS; row++)
        {
            for (col = 0; col < TEXT_COLS; col++)
            {
                byte code = (byte)fade_grid[row][col];

                if (code > ' ')
                {
                    if (code > (byte)' ' + FADE_STEP)
                        code -= FADE_STEP;
                    else
                        code = ' ';

                    fade_grid[row][col] = (char)code;
                    writeCharAt_42cols(col, row, code);
                    any = 1;
                }
            }
        }

        wait_ticks(FADE_TICKS);
    } while (any);
}
