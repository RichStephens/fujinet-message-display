/*
 * qr_irata.c - Display IRATA.ONLINE QR code using IBM PC block graphics
 *              Encodes: https://qrco.de/bgoKBY
 *
 * Build with Watcom C (16-bit DOS target):
 *   wcl -ml qr_irata.c
 *
 * Build with Watcom C (32-bit DOS4GW target):
 *   wcl386 qr_irata.c
 *
 * Renders a 33x33 module QR code (with 4-module quiet zone) using
 * CP437 half-block characters, packing two module rows per text row:
 *   0xDB = Full block  (both modules dark)
 *   0xDF = Upper half  (top dark,  bottom light)
 *   0xDC = Lower half  (top light, bottom dark)
 *   0x20 = Space       (both modules light)
 *
 * All cells use attribute 0x70: black foreground on white background.
 * The upper half-block character therefore renders as a black square on
 * the top half and white on the bottom, giving crisp 1-module fidelity
 * at the cost of only 17 character rows and 33 character columns.
 *
 * Screen layout (80x25):
 *   Horizontal: centered at column 23  (33 chars wide)
 *   Vertical:   centered at row 4      (17 char rows tall)
 */

#include <conio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* QR module bitmap: 33x33 (25x25 data + 4-module quiet zone border)  */
/* 1 = dark module, 0 = light module                                   */
/* ------------------------------------------------------------------ */
#define QR_SIZE 33

static const unsigned char qr[QR_SIZE][QR_SIZE] = {
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,1,1,0,0,1,0,0,0,0,1,0,1,0,1,1,1,1,1,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,0,0,0,0,1,0,1,1,0,1,1,1,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,0,1,1,1,0,0,0,0,0,0,1,0,1,1,1,0,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,1,1,0,1,0,1,0,1,1,0,1,0,1,1,1,0,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,0,0,1,0,1,0,0,1,1,0,1,0,1,1,1,0,1,0,0,0,0 },
    { 0,0,0,0,1,0,0,0,0,0,1,0,1,0,1,0,0,1,1,1,0,0,1,0,0,0,0,0,1,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1,0,1,0,1,0,1,0,0,0,0,0 },
    { 0,0,0,0,1,0,0,0,0,1,0,1,1,1,0,0,0,0,1,1,1,1,0,1,1,0,0,0,1,0,0,0,0 },
    { 0,0,0,0,0,0,1,1,0,0,1,1,0,1,0,1,1,1,1,1,0,1,1,0,1,0,1,0,0,0,0,0,0 },
    { 0,0,0,0,1,1,0,1,0,1,0,0,1,1,1,1,0,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,1,1,1,0,0,1,0,1,1,0,1,0,1,1,1,0,0,1,1,1,1,0,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,0,1,0,1,0,0,1,1,1,0,1,0,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,0,1,0,0,1,1,0,1,0,1,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,0,1,0,1,0,0,1,0,1,1,0,0,0,1,0,0,0,0,0,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,0,0,1,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,0,1,0,1,0,1,0,1,0,1,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,1,0,1,1,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,1,0,0,0,1,0,0,1,1,0,1,0,1,0,0,1,1,0,0,0,0 },
    { 0,0,0,0,1,0,1,1,1,0,1,0,1,0,1,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,0,0,0 },
    { 0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0,1,1,1,0,1,1,1,1,1,1,0,1,0,0,0,0,0 },
    { 0,0,0,0,1,1,1,1,1,1,1,0,1,0,1,0,0,0,0,1,0,0,0,0,0,0,1,1,1,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};

/* ------------------------------------------------------------------ */
/* CP437 half-block characters                                         */
/* ------------------------------------------------------------------ */
#define CHAR_FULL  0xDB   /* █  both halves dark                       */
#define CHAR_UPPER 0xDF   /* ▀  upper dark, lower light               */
#define CHAR_LOWER 0xDC   /* ▄  upper light, lower dark               */
#define CHAR_BLANK 0x20   /* space - both halves light                 */

/*
 * Attribute 0x70: foreground = black (0), background = white (7).
 * The half-block char's glyph pixels are drawn in foreground (black),
 * the background fills the other half, so:
 *   CHAR_UPPER | ATTR: top=black(dark), bottom=white(light)   OK
 *   CHAR_LOWER | ATTR: top=white(light), bottom=black(dark)   OK
 *   CHAR_FULL  | ATTR: all black (dark)                        OK
 *   CHAR_BLANK | ATTR: all white background (light)            OK
 */
#define ATTR_QR    0x70
#define ATTR_CLEAR 0x70

/* Screen layout */
#define SCREEN_COLS  80
#define SCREEN_ROWS  25
#define QR_CHAR_COLS QR_SIZE                  /* 33 character columns  */
#define QR_CHAR_ROWS ((QR_SIZE + 1) / 2)     /* 17 character rows     */
#define START_COL    ((SCREEN_COLS - QR_CHAR_COLS) / 2)  /* 23        */
#define START_ROW    ((SCREEN_ROWS - QR_CHAR_ROWS) / 2)  /* 4         */

/* Video memory: 0xB800:0000 in segmented form */
#define VIDEO_SEG    0xB800U

int main(void)
{
    unsigned char far *video;
    unsigned int  pos;
    int           row, col;
    int           mod_row;        /* module row pair index (0..16)     */
    int           top, bot;       /* module values for upper/lower half */
    unsigned char ch;

    /* Point at CGA/EGA/VGA text-mode video buffer */
    video = (unsigned char far *)((unsigned long)VIDEO_SEG << 16);

    /* Fill entire screen with white background */
    for (pos = 0; pos < (unsigned int)(SCREEN_COLS * SCREEN_ROWS * 2); pos += 2) {
        video[pos]     = CHAR_BLANK;
        video[pos + 1] = ATTR_CLEAR;
    }

    /* Draw QR code using half-block technique */
    for (mod_row = 0; mod_row < QR_CHAR_ROWS; mod_row++) {
        row = (START_ROW + mod_row) * SCREEN_COLS * 2;

        for (col = 0; col < QR_CHAR_COLS; col++) {
            int mod_top = mod_row * 2;
            int mod_bot = mod_top + 1;

            top = qr[mod_top][col];
            bot = (mod_bot < QR_SIZE) ? qr[mod_bot][col] : 0;

            if      ( top && bot)  ch = CHAR_FULL;
            else if ( top && !bot) ch = CHAR_UPPER;
            else if (!top && bot)  ch = CHAR_LOWER;
            else                   ch = CHAR_BLANK;

            pos = (unsigned int)(row + (START_COL + col) * 2);
            video[pos]     = ch;
            video[pos + 1] = ATTR_QR;
        }
    }

    /* Wait for any keypress before returning */
    getch();
    return 0;
}
