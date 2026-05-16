/**
 * @brief   Message display - MS-DOS (40x25 text mode, CP437 half-block QR)
 * @license gpl v. 3, see LICENSE for details
 */

#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <i86.h>
#include "qrcode.h"

#define VIDEO_SEG    0xB800
#define SCREEN_COLS  40
#define SCREEN_ROWS  25
#define SCREEN_CELLS (SCREEN_COLS * SCREEN_ROWS)

#define ATTR_QR      0x70   /* black on white - QR must stay scannable */
#define ATTR_SCREEN  0x1F   /* bright white on blue */

#define CHAR_FULL    0xDB
#define CHAR_UPPER   0xDF
#define CHAR_LOWER   0xDC
#define CHAR_BLANK   0x20

#define BOX_TL       0xC9   /* CP437 double-line box-drawing characters */
#define BOX_TR       0xBB
#define BOX_BL       0xC8
#define BOX_BR       0xBC
#define BOX_HZ       0xCD
#define BOX_VT       0xBA

#define QR_CHAR_COLS QR_SIZE
#define QR_CHAR_ROWS ((QR_SIZE + 1) / 2)
#define QR_START_COL ((SCREEN_COLS - QR_CHAR_COLS) / 2)
#define QR_START_ROW 4

#define TEXT_X       2     /* message text area, inside the box border */
#define TEXT_Y       2
#define TEXT_W       36
#define TEXT_H       22

#define KEY_ESC      27
#define ESC_WINDOW   18     /* ~1 second at 18.2 Hz */
#define ESC_NEEDED   3

#define QR_HOLD      36     /* ticks */
#define MSG_HOLD     90

enum { SCREEN_NONE, SCREEN_QR, SCREEN_MSG };

static unsigned char far *vram;
static int screen_ready = 0;
static int current_screen = SCREEN_NONE;
static unsigned char saved_mode = 0x03;
static unsigned int anim_index = 0;

static unsigned long esc_first = 0;
static int esc_count = 0;

static unsigned long get_ticks(void)
{
    unsigned long far *t = (unsigned long far *)MK_FP(0x0040, 0x006C);
    return *t;
}

static unsigned char get_mode(void)
{
    union REGS r;
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    return r.h.al;
}

static void set_mode(unsigned char m)
{
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = m;
    int86(0x10, &r, &r);
}

static void set_cursor(int visible)
{
    union REGS r;
    r.h.ah = 0x01;
    r.h.bh = 0x00;
    if (visible)
    {
        r.h.ch = 0x06;
        r.h.cl = 0x07;
    }
    else
    {
        r.h.ch = 0x20;
        r.h.cl = 0x00;
    }
    int86(0x10, &r, &r);
}

static void restore_screen(void)
{
    set_mode(saved_mode);
    set_cursor(1);
}

static void screen_init(void)
{
    if (screen_ready)
        return;

    saved_mode = get_mode();
    set_mode(0x01);            /* 40x25 16-color text */
    set_cursor(0);
    atexit(restore_screen);

    vram = (unsigned char far *)MK_FP(VIDEO_SEG, 0);
    screen_ready = 1;
}

static void put_cell(int col, int row, unsigned char ch, unsigned char attr)
{
    unsigned int pos = (unsigned int)(((row * SCREEN_COLS) + col) << 1);
    vram[pos]     = ch;
    vram[pos + 1] = attr;
}

static void fill_screen(unsigned char ch, unsigned char attr)
{
    unsigned int pos;
    for (pos = 0; pos < (unsigned int)(SCREEN_CELLS << 1); pos += 2)
    {
        vram[pos]     = ch;
        vram[pos + 1] = attr;
    }
}

static void draw_centered(int row, const char *s, unsigned char attr)
{
    int len = (int)strlen(s);
    int col = (SCREEN_COLS - len) / 2;
    int i;
    for (i = 0; i < len; i++)
        put_cell(col + i, row, (unsigned char)s[i], attr);
}

static void draw_box(void)
{
    int right  = SCREEN_COLS - 1;
    int bottom = SCREEN_ROWS - 1;
    int x, y;

    put_cell(0,     0,      BOX_TL, ATTR_SCREEN);
    put_cell(right, 0,      BOX_TR, ATTR_SCREEN);
    put_cell(0,     bottom, BOX_BL, ATTR_SCREEN);
    put_cell(right, bottom, BOX_BR, ATTR_SCREEN);

    for (x = 1; x < right; x++)
    {
        put_cell(x, 0,      BOX_HZ, ATTR_SCREEN);
        put_cell(x, bottom, BOX_HZ, ATTR_SCREEN);
    }
    for (y = 1; y < bottom; y++)
    {
        put_cell(0,     y, BOX_VT, ATTR_SCREEN);
        put_cell(right, y, BOX_VT, ATTR_SCREEN);
    }
}

static void note_esc(void)
{
    unsigned long now = get_ticks();

    if (esc_count == 0 || (now - esc_first) > ESC_WINDOW)
    {
        esc_count = 1;
        esc_first = now;
    }
    else
    {
        esc_count++;
    }

    if (esc_count >= ESC_NEEDED)
        exit(0);
}

static void drain_keys(void)
{
    int k;
    while (kbhit())
    {
        k = getch();
        if (k == 0)            /* extended key: discard scan code */
        {
            getch();
            continue;
        }
        if (k == KEY_ESC)
            note_esc();
    }
}

static void poll_wait(unsigned long ticks)
{
    unsigned long start = get_ticks();
    while ((get_ticks() - start) < ticks)
        drain_keys();
}

void show_qr_code(void)
{
    int mod_row, col, mod_top, mod_bot, top, bot;
    unsigned char ch;

    screen_init();

    if (current_screen != SCREEN_QR)
    {
        fill_screen(CHAR_BLANK, ATTR_SCREEN);

        for (mod_row = 0; mod_row < QR_CHAR_ROWS; mod_row++)
        {
            for (col = 0; col < QR_CHAR_COLS; col++)
            {
                mod_top = mod_row * 2;
                mod_bot = mod_top + 1;

                top = qr[mod_top][col];
                bot = (mod_bot < QR_SIZE) ? qr[mod_bot][col] : 0;

                if      ( top &&  bot) ch = CHAR_FULL;
                else if ( top && !bot) ch = CHAR_UPPER;
                else if (!top &&  bot) ch = CHAR_LOWER;
                else                   ch = CHAR_BLANK;

                put_cell(QR_START_COL + col, QR_START_ROW + mod_row, ch, ATTR_QR);
            }
        }

        draw_centered(22, "SCAN QR CODE", ATTR_SCREEN);
        draw_centered(23, "TO SEND A MESSAGE", ATTR_SCREEN);

        current_screen = SCREEN_QR;
    }

    poll_wait(QR_HOLD);
}

static void anim_melt(void)
{
    int col_delay[SCREEN_COLS];
    int col_shift[SCREEN_COLS];
    int c, r, done;
    unsigned int rng = (unsigned int)get_ticks() | 1;

    for (c = 0; c < SCREEN_COLS; c++)
    {
        rng = (rng * 25173u) + 13849u;
        col_delay[c] = (int)((rng >> 8) & 15);
        col_shift[c] = 0;
    }

    do
    {
        done = 1;
        for (c = 0; c < SCREEN_COLS; c++)
        {
            if (col_shift[c] >= SCREEN_ROWS)
                continue;

            done = 0;

            if (col_delay[c] > 0)
            {
                col_delay[c]--;
                continue;
            }

            for (r = SCREEN_ROWS - 1; r > 0; r--)
            {
                unsigned int dst = (unsigned int)(((r * SCREEN_COLS) + c) << 1);
                unsigned int src = (unsigned int)((((r - 1) * SCREEN_COLS) + c) << 1);
                vram[dst]     = vram[src];
                vram[dst + 1] = vram[src + 1];
            }
            vram[(unsigned int)(c << 1)]       = CHAR_BLANK;
            vram[(unsigned int)((c << 1) + 1)] = ATTR_SCREEN;
            col_shift[c]++;
        }
        poll_wait(1);
    } while (!done);
}

static void anim_dissolve(void)
{
    unsigned int lfsr = 0x2C9;
    unsigned int i;
    unsigned int cleared = 0;

    /* cell 0 never appears in the LFSR sequence; clear it up front */
    vram[0] = CHAR_BLANK;
    vram[1] = ATTR_SCREEN;

    for (i = 0; i < 1023; i++)
    {
        unsigned int bit = ((lfsr >> 9) ^ (lfsr >> 6)) & 1;
        lfsr = ((lfsr << 1) | bit) & 0x3FF;

        if (lfsr < SCREEN_CELLS)
        {
            unsigned int pos = (unsigned int)(lfsr << 1);
            vram[pos]     = CHAR_BLANK;
            vram[pos + 1] = ATTR_SCREEN;
            cleared++;
            if ((cleared & 15) == 0)
                poll_wait(1);
        }
    }
}

static void anim_implode(void)
{
    int k, c, r;

    for (k = 0; k <= SCREEN_ROWS / 2; k++)
    {
        int top   = k;
        int bot   = SCREEN_ROWS - 1 - k;
        int left  = k;
        int right = SCREEN_COLS - 1 - k;

        if (top > bot || left > right)
            break;

        for (c = left; c <= right; c++)
        {
            put_cell(c, top, CHAR_BLANK, ATTR_SCREEN);
            put_cell(c, bot, CHAR_BLANK, ATTR_SCREEN);
        }
        for (r = top; r <= bot; r++)
        {
            put_cell(left,  r, CHAR_BLANK, ATTR_SCREEN);
            put_cell(right, r, CHAR_BLANK, ATTR_SCREEN);
        }
        poll_wait(3);
    }

    fill_screen(CHAR_BLANK, ATTR_SCREEN);
}

void display_message(const char *msg)
{
    int row, col, i, wlen, k;
    char c;

    screen_init();

    fill_screen(CHAR_BLANK, ATTR_SCREEN);
    draw_box();
    draw_centered(0, " MESSAGE RECEIVED ", ATTR_SCREEN);

    row = 0;
    col = 0;
    i = 0;
    while (msg[i] != '\0' && row < TEXT_H)
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
            if (col > 0 && col < TEXT_W)
                col++;
            i++;
            continue;
        }

        wlen = 0;
        while (msg[i + wlen] != '\0' && msg[i + wlen] != ' ' && msg[i + wlen] != '\n')
            wlen++;

        if (col > 0 && col + wlen > TEXT_W)
        {
            row++;
            col = 0;
        }

        for (k = 0; k < wlen && row < TEXT_H; k++)
        {
            c = msg[i + k];
            if (c < 0x20 || c > 0x7E)
                continue;
            if (col >= TEXT_W)
            {
                row++;
                col = 0;
                if (row >= TEXT_H)
                    break;
            }
            put_cell(TEXT_X + col, TEXT_Y + row, (unsigned char)c, ATTR_SCREEN);
            col++;
        }
        i += wlen;
    }

    current_screen = SCREEN_MSG;

    poll_wait(MSG_HOLD);

    switch (anim_index % 3)
    {
    case 0:  anim_melt();     break;
    case 1:  anim_dissolve(); break;
    default: anim_implode();  break;
    }
    anim_index++;

    current_screen = SCREEN_NONE;
}
