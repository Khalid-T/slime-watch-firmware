/* THE WATCH PAGE -- page.h says what; this is how.
 *
 * THE TOOL PAGE (240 x 240, the centre at 120,120; everything inside
 * CLEAR_R 112, under the night ring's inner edge at 113, so the rings
 * stay; every block pinned, nothing turns with gravity):
 *
 *            .  .  .  12  .  .  .
 *        .           (o)           .        the LINK dot, r 80 above the centre
 *      .                             .
 *     .                               .
 *    .            07:41                .    the HOUR, the glance's 3x5 font at 4 px (76 x 20)
 *    .          TUE 10 SEP             .    the DATE, the same font grown an uppercase set, 3 px
 *    .      [foot] 4213  [drop] 87%    .    the STEPS and the CELL, a 5x5 mark each, 3 px
 *     .           [bell] 07:30        .     the NEXT ALARM, or --
 *      .            STORM            .      the SKY's word, when one is held
 *        .                         .
 *            ` .  .  . 6 .  .  . '
 *
 * While a stopwatch or a timer runs (THE PAGE GROWS, 2026-09-10
 * evening) the DATE row is the tool's row instead -- a 5x5 mark (the
 * stopwatch's ring, the timer's hourglass) and its M:SS, the lap count
 * after it -- and the SKY row shows the last two laps' splits when there
 * are any: the date and the sky are the ordinary day's; a count running
 * is what he opened the page for.
 *
 * HIS WEEK (a second drag up): seven rows, one a day, oldest at the top
 * and today at the bottom -- the day's letters, a bar against the
 * week's best, the number; a day with no file is "--". Above them the
 * night's hours as H:MM with a moon (rest.cpp's last night; "--" when
 * none is known), below them the cell with the drop. THE NOTICES (a
 * third drag up): the last three wisps from the desk, newest first --
 * the app's last name and how long ago on one line, the title dimmed
 * under it; none = "NO NOTICES". SETTINGS 1 to 5 (the fourth to the
 * eighth drag up; 3-5 since MORE SETTINGS, 09-10 21:12): a small title,
 * then rows he taps -- the label at the row's left, its state (ON /
 * OFF, HERE / AWAY, HOLD / AUTO, LOW / MID / HIGH, 12H / 24H, an hour)
 * at its right, a tapped row lit in the shine with a bar under it for
 * SET_HILITE_S. ABOUT (a ninth): six read-only lines. A tenth drag up
 * is the tool page again. THE SWIPES (design/watch-swipes.md 3A,
 * 2026-09-23): a flick along his wall turns the ring too -- toward his
 * left forward (page_next, as the drag up), toward his right BACK
 * (page_prev: from the tool page, ABOUT); main reads the flick's way.
 *
 * THE OPERATOR NIGHT (2026-10-12): THE BLUETOOTH PAGE (T16) sits after
 * THE NOTICES -- five lines in words, the labels dimmed, the state at
 * the right (PHONE NEAR / AWAY / NONE, AIR DOOR OPEN / SHUT, EAR ON /
 * OFF / NO BOND, PAIRING OPEN / SHUT, NOTICES SOME / NONE) at a pitch
 * of BLE_ROW_H so five fit the disc; PAIRING is its one row (a tap =
 * the window), a tap anywhere else keeps the page 8 s as ABOUT does.
 * SETTINGS 6 (T15, THE HIDDEN SWITCHES) after 5: PULSE AUTO, DOCK HAND,
 * EAR GEAR and FORGET -- the last armed by one tap (SURE? in the shine
 * for FORGET_ARM_S, the page's own memory as a lit row is) and done by
 * a second inside it; the arm drops with the page, or when it turns.
 *
 * Inks: the hour, the numbers and the alarm in the shine; the date, the
 * sky's word, the titles and the splits in the shine dimmed; the marks
 * in the body colour; the alarm's "--" in the shade; the link dot the
 * shine pushed green when a door is there (or the radio rests on its
 * budget), the shade when none is. The 3x5 font is the glance's, with
 * the uppercase set added here (each drawer keeps its own table: the
 * host tests build them alone). */
#include "page.h"
#include "hourfmt.h"    /* MORE SETTINGS' CLOCK row: every hour on the page through one formatter */
#include "gel.h"
#include "theme.h"      /* THE THEME: the palette the slime wears is the glass's (wardrobe) */        /* the palette */
#include "mark.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --- the tunables ------------------------------------------------------- */
static const int   W = 240, H = 240;
static const int   CLEAR_R = 112;       /* the page's disc: inside the night ring (r 113-115) */
static const int   PX_HOUR = 4;         /* the hour's block */
static const int   PX = 3;              /* everything else's */
static const int   DOT_CY = 40;         /* the link dot's centre row */
static const int   HOUR_Y = 74;         /* the rows: each block's top */
static const int   DATE_Y = 104;
static const int   ROW1_Y = 134;        /* steps | cell */
static const int   ROW2_Y = 160;        /* the alarm */
static const int   ROW3_Y = 186;        /* the sky, or the laps */
static const int   ROW1_LX = 72, ROW1_RX = 168;   /* the two groups' centres on the steps row */
static const float DIM_K = 0.75f;       /* the date's and the sky's ink */
/* HIS WEEK: the night's row, then seven day rows WEEK_ROW_H apart, then the cell's row */
static const int   WEEK_NIGHT_Y = 40;
static const int   WEEK_Y0 = 62;
static const int   WEEK_ROW_H = 18;
static const int   WEEK_CELL_Y = 196;
static const int   WEEK_BAR_MAX = 66;   /* the longest bar, px (the week's best day) */
static const int   WEEK_BAR_X = 78;     /* where the bars begin (after the day's three letters) */
/* THE NOTICES: three, two lines each; a row's tap band is its pitch, NOTE_TAP_PAD above its first line */
static const int   NOTE_Y0 = 62;
static const int   NOTE_ROW_H = 40;
static const int   NOTE_APP_MAX = 8;    /* glyphs of the app's name (or the id's last word) on the list */
static const int   NOTE_MARGIN = 6;     /* px kept from the page's rim at a row */
static const int   NOTE_TAP_PAD = 4;    /* the band: 4 above the name's blocks to 4 above the next row's */
/* THE NOTICE OPENED: the name, the title, the message's rows, the age */
static const int   OPEN_NAME_Y = 40;
static const int   OPEN_TITLE_Y = 62;
static const int   OPEN_MSG_Y0 = 86;
static const int   OPEN_MSG_H = 18;     /* the rows' pitch (the week's) */
static const int   OPEN_AGE_Y = 190;
/* THE SETTINGS PAGE: a title, then a page's rows SET_ROW_H apart; a
 * tap lands on a row within SET_ROW_PAD above and below its blocks (the
 * whole pitch is the finger's: 34 px, ~6 mm on this glass) */
static const int   SET_TITLE_Y = 38;
static const int   SET_Y0 = 66;
static const int   SET_ROW_H = 34;
static const int   SET_ROW_PAD = 9;     /* the hit band: 9 + 15 + 10 = the pitch */
/* the settings pages' rows: where each page's run begins in SET_* and how many (four at most: a fifth row's chord
 * at y 202 is 100 px, too short for a label and its state). SETTINGS 1: SLIME, LEASE, QUIET TIME, FACE; 2: WATER
 * LOCK, LINK NOW, ALARM; 3: WIFI, NETWORK, ADDRESS, BLUETOOTH; 4: BRIGHTNESS, LIT SPELL, LAMP; 5: CLOCK, QUIET FROM,
 * QUIET TO (MORE SETTINGS, 09-10 21:12; THE SLIME ROW took page 1's first place and ALARM moved to page 2);
 * 4 gained BUMPS (THE BUMPS' switch) as its fourth row on 2026-09-23 -- under LAMP, the rows above it untouched */
static const int   SET_PAGE_FIRST[PAGE_SETTINGS_N] = { SET_SLIME, SET_LOCK, SET_WIFI, SET_BRIGHT, SET_CLOCK, SET_PAUTO };
static const int   SET_PAGE_ROWS[PAGE_SETTINGS_N]  = { 4, 3, 4, 4, 4, 4 };   /* SETTINGS 4 gained BUMPS as its fourth row (2026-09-23); 5 gained PULSE as its (2026-10-03); 6 (T15, 2026-10-12): PULSE AUTO, DOCK HAND, EAR GEAR, FORGET */
/* THE BLUETOOTH PAGE: a title, five lines at a pitch of BLE_ROW_H (34 would put the fifth at y 202, a 100 px chord: too
 * short for NOTICES NONE); the PAIRING line (BLE_PAIR_ROW) is the one row, its tap band the settings' (SET_ROW_PAD) */
static const int   BLE_Y0 = 62;
static const int   BLE_ROW_H = 27;
static const int   BLE_ROWS = 5;
static const int   BLE_PAIR_ROW = 3;
/* ABOUT: six read-only lines (a label left, a number right) at the week's pitch -- no tap targets, so no 34 px bands */
static const int   ABOUT_Y0 = 58;
static const int   ABOUT_H  = 22;

/* THE LAMP: the whole glass (the rim's radius, the rings under it too:
 * a lamp is not a page), white */
static const int   LAMP_R = 120;
static const uint16_t WHITE = 0xFFFFu;

/* --- the state ---------------------------------------------------------- */
static bool  up = false;
static int   which = PAGE_TOOL;
static float left_s = 0;
static bool  lamp = false;              /* THE LAMP burns */
static float lamp_s = 0;                /* ...this much longer */
static float t_s = 0;                   /* the page's clock (the cell's breath on the dock) */
static int   hilite_row = -1;           /* THE SETTINGS PAGE: the row a tap lit... */
static float hilite_s = 0;              /* ...for this much longer */
static int   note_open = -1;            /* THE NOTICE OPENED: the row open on THE NOTICES, -1 = the list */
static int   notes_shown = 0;           /* ...and how many rows the last frame drew (the tap lands on what he sees) */
static float forget_s = 0;              /* SETTINGS 6's FORGET: armed by one tap for this much longer (the page's own memory; page_tick drops it off its page) */
/* BATTERY MANNERS: on the dock the cell's number BREATHES -- its ink between
 * CELL_BREATH_LOW and full over CELL_BREATH_S, the face's own drop pulse
 * (the cell drinking); under 10 % the number is the shade */
static const float CELL_BREATH_S   = 2.0f;
static const float CELL_BREATH_LOW = 0.45f;
static char  cause[24] = "";
static char  logline[96];
static bool  log_new = false;
static mark_t mk;
bool page_mark(dirty_rect_t *box) { return mark_take(&mk, box); }

/* --- the font: 3x5, msb left ---------------------------------------------- */
static const uint8_t DIGITS[10][5] = {
    { 0b111, 0b101, 0b101, 0b101, 0b111 },  /* 0 */
    { 0b010, 0b110, 0b010, 0b010, 0b111 },  /* 1 */
    { 0b111, 0b001, 0b111, 0b100, 0b111 },  /* 2 */
    { 0b111, 0b001, 0b111, 0b001, 0b111 },  /* 3 */
    { 0b101, 0b101, 0b111, 0b001, 0b001 },  /* 4 */
    { 0b111, 0b100, 0b111, 0b001, 0b111 },  /* 5 */
    { 0b111, 0b100, 0b111, 0b101, 0b111 },  /* 6 */
    { 0b111, 0b001, 0b010, 0b010, 0b010 },  /* 7 */
    { 0b111, 0b101, 0b111, 0b101, 0b111 },  /* 8 */
    { 0b111, 0b101, 0b111, 0b001, 0b111 },  /* 9 */
};
static const uint8_t LETTERS[26][5] = {
    { 0b010, 0b101, 0b111, 0b101, 0b101 },  /* A */
    { 0b110, 0b101, 0b110, 0b101, 0b110 },  /* B */
    { 0b111, 0b100, 0b100, 0b100, 0b111 },  /* C */
    { 0b110, 0b101, 0b101, 0b101, 0b110 },  /* D */
    { 0b111, 0b100, 0b110, 0b100, 0b111 },  /* E */
    { 0b111, 0b100, 0b110, 0b100, 0b100 },  /* F */
    { 0b111, 0b100, 0b101, 0b101, 0b111 },  /* G */
    { 0b101, 0b101, 0b111, 0b101, 0b101 },  /* H */
    { 0b111, 0b010, 0b010, 0b010, 0b111 },  /* I */
    { 0b001, 0b001, 0b001, 0b101, 0b010 },  /* J */
    { 0b101, 0b101, 0b110, 0b101, 0b101 },  /* K */
    { 0b100, 0b100, 0b100, 0b100, 0b111 },  /* L */
    { 0b101, 0b111, 0b111, 0b101, 0b101 },  /* M */
    { 0b111, 0b101, 0b101, 0b101, 0b101 },  /* N */
    { 0b111, 0b101, 0b101, 0b101, 0b111 },  /* O */
    { 0b111, 0b101, 0b111, 0b100, 0b100 },  /* P */
    { 0b111, 0b101, 0b101, 0b111, 0b001 },  /* Q */
    { 0b110, 0b101, 0b110, 0b101, 0b101 },  /* R */
    { 0b111, 0b100, 0b111, 0b001, 0b111 },  /* S */
    { 0b111, 0b010, 0b010, 0b010, 0b010 },  /* T */
    { 0b101, 0b101, 0b101, 0b101, 0b111 },  /* U */
    { 0b101, 0b101, 0b101, 0b101, 0b010 },  /* V */
    { 0b101, 0b101, 0b111, 0b111, 0b101 },  /* W */
    { 0b101, 0b101, 0b010, 0b101, 0b101 },  /* X */
    { 0b101, 0b101, 0b010, 0b010, 0b010 },  /* Y */
    { 0b111, 0b001, 0b010, 0b100, 0b111 },  /* Z */
};
static const uint8_t COLON[5]   = { 0b000, 0b010, 0b000, 0b010, 0b000 };
static const uint8_t PERCENT[5] = { 0b101, 0b001, 0b010, 0b100, 0b101 };
static const uint8_t DASH[5]    = { 0b000, 0b000, 0b111, 0b000, 0b000 };
static const uint8_t PERIOD[5]  = { 0b000, 0b000, 0b000, 0b000, 0b010 };   /* the ip's dots (SETTINGS 3, ABOUT) */
static const uint8_t PLUS[5]    = { 0b000, 0b010, 0b111, 0b010, 0b000 };   /* ABOUT's build id: `+` = the tree was dirty at the build */
static const uint8_t QUERY[5]   = { 0b111, 0b001, 0b010, 0b000, 0b010 };   /* SETTINGS 6's FORGET armed: `SURE?` */
static const uint8_t BLANK[5]   = { 0, 0, 0, 0, 0 };
/* the marks, 5x5 */
static const uint8_t FOOT[5]  = { 0b01010, 0b10101, 0b01110, 0b11111, 0b01110 };   /* a footprint: the toes, the sole */
static const uint8_t DROP[5]  = { 0b00100, 0b01110, 0b11111, 0b11111, 0b01110 };   /* the cell's drop */
static const uint8_t BELL[5]  = { 0b00100, 0b01110, 0b01110, 0b11111, 0b00100 };   /* the alarm's bell */
static const uint8_t DOT[5]   = { 0b01110, 0b11111, 0b11111, 0b11111, 0b01110 };   /* the link */
static const uint8_t WATCH[5] = { 0b00100, 0b01110, 0b10101, 0b10011, 0b01110 };   /* the stopwatch: a ring with its hand */
static const uint8_t GLASS[5] = { 0b11111, 0b01110, 0b00100, 0b01110, 0b11111 };   /* the timer: an hourglass */
static const uint8_t MOON[5]  = { 0b01100, 0b11000, 0b10000, 0b11000, 0b01100 };   /* the night: a crescent */

static const char *const DAYS[7]    = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
static const char *const MONTHS[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

static const uint8_t *glyph(char c)
{
    if (c >= '0' && c <= '9') return DIGITS[c - '0'];
    if (c >= 'A' && c <= 'Z') return LETTERS[c - 'A'];
    if (c >= 'a' && c <= 'z') return LETTERS[c - 'a'];
    if (c == ':') return COLON;
    if (c == '%') return PERCENT;
    if (c == '-') return DASH;
    if (c == '.') return PERIOD;
    if (c == '+') return PLUS;
    if (c == '?') return QUERY;
    return BLANK;
}

/* --- the ink ------------------------------------------------------------- */
static inline uint16_t ink(const gel_rgba_t *c, float k)
{
    int R = (int)(c->r * k * 31 + 0.5f), G = (int)(c->g * k * 63 + 0.5f), B = (int)(c->b * k * 31 + 0.5f);
    uint16_t v = (uint16_t)((R << 11) | (G << 5) | B);
    return (uint16_t)((v >> 8) | (v << 8));            /* the panel's byte order */
}

static void block(uint16_t *fb, int x, int y, int s, uint16_t c)
{
    mark_rect(&mk, x, y, s, s, c);
    for (int yy = y; yy < y + s; yy++) {
        if (yy < 0 || yy >= H) continue;
        for (int xx = x; xx < x + s; xx++) {
            if (xx < 0 || xx >= W) continue;
            fb[yy * W + xx] = c;
        }
    }
}
/* a filled rectangle (the week's bars) */
static void rect(uint16_t *fb, int x, int y, int w, int h, uint16_t c)
{
    if (w <= 0 || h <= 0) return;
    mark_rect(&mk, x, y, w, h, c);
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= H) continue;
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= W) continue;
            fb[yy * W + xx] = c;
        }
    }
}

/* a string in the 3x5 font at ``px``, its left edge at x0; returns the width in px */
static int text_w(const char *s, int px) { int n = (int)strlen(s); return n ? (n * 4 - 1) * px : 0; }
static int text(uint16_t *fb, int x0, int y0, int px, const char *s, uint16_t c)
{
    for (int i = 0; s[i]; i++) {
        const uint8_t *g = glyph(s[i]);
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 3; col++)
                if (g[row] & (1 << (2 - col)))
                    block(fb, x0 + (i * 4 + col) * px, y0 + row * px, px, c);
    }
    return text_w(s, px);
}
static void text_centred(uint16_t *fb, int cx, int y0, int px, const char *s, uint16_t c)
{
    text(fb, cx - text_w(s, px) / 2, y0, px, s, c);
}
/* a 5x5 mark at ``px``, its left edge at x0 */
static void mark5(uint16_t *fb, int x0, int y0, int px, const uint8_t *g, uint16_t c)
{
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 5; col++)
            if (g[row] & (1 << (4 - col)))
                block(fb, x0 + col * px, y0 + row * px, px, c);
}
/* a mark, a gap of one block, a string -- the group centred on cx */
static void mark_text(uint16_t *fb, int cx, int y0, const uint8_t *g, uint16_t gc, const char *s, uint16_t c)
{
    int w = 5 * PX + PX + text_w(s, PX);
    int x0 = cx - w / 2;
    mark5(fb, x0, y0, PX, g, gc);
    text(fb, x0 + 6 * PX, y0, PX, s, c);
}
/* the page's half-width at a row of blocks (the chord of its disc through
 * the row's farthest edge from the centre), less a margin */
static int half_at(int y0, int rows_px)
{
    int top = y0, bot = y0 + rows_px;
    int dy = (H / 2 - top) > (bot - H / 2) ? (H / 2 - top) : (bot - H / 2);
    if (dy < 0) dy = 0;
    if (dy >= CLEAR_R) return 0;
    return (int)sqrtf((float)(CLEAR_R * CLEAR_R - dy * dy)) - NOTE_MARGIN;
}
/* a string cut to fit ``max_px`` at ``px`` */
static void cut_to(char *s, int max_px, int px)
{
    int n = (int)strlen(s);
    while (n > 0 && (n * 4 - 1) * px > max_px) s[--n] = 0;
}
static void mss(char *s, int cap, uint32_t ms, bool up_)
{
    uint32_t sec = up_ ? (ms + 999) / 1000 : ms / 1000;
    snprintf(s, cap, "%lu:%02lu", (unsigned long)((sec / 60) % 100), (unsigned long)(sec % 60));
}
static void ago(char *s, int cap, uint32_t age_s)
{
    if (age_s < 60) snprintf(s, cap, "%luS", (unsigned long)age_s);
    else if (age_s < 3600) snprintf(s, cap, "%luM", (unsigned long)(age_s / 60));
    else if (age_s < 86400) snprintf(s, cap, "%luH", (unsigned long)(age_s / 3600));
    else snprintf(s, cap, "%luD", (unsigned long)(age_s / 86400));
}

/* --- the door ------------------------------------------------------------- */
static const char *const PAGE_NAMES[PAGE_N] = { "the tool page", "his week", "the notices", "bluetooth", "settings 1/6", "settings 2/6", "settings 3/6", "settings 4/6", "settings 5/6", "settings 6/6", "about" };
/* THE SETTINGS PAGE's rows: the label on the glass, the console's name */
static const char *const SET_LABELS[SET_N] = { "SLIME", "LEASE", "QUIET TIME", "FACE", "WATER LOCK", "LINK NOW", "ALARM",
                                               "WIFI", "NETWORK", "ADDRESS", "BLUETOOTH", "BRIGHTNESS", "LIT SPELL", "LAMP", "BUMPS",
                                               "CLOCK", "QUIET FROM", "QUIET TO", "PULSE", "ABOUT",
                                               "PULSE AUTO", "DOCK HAND", "EAR GEAR", "FORGET",       /* SETTINGS 6 (T15) */
                                               "PAIRING", "BLUETOOTH" };                              /* THE BLUETOOTH PAGE (T16): its row, its disc */
static const char *const SET_NAMES[SET_N]  = { "slime", "lease", "quiet", "face", "lock", "link", "alarm",
                                               "wifi", "network", "address", "bt", "bright", "spell", "lamp", "bumps",
                                               "clock", "qfrom", "qto", "pulse", "about",
                                               "pauto", "dockhand", "eargear", "forget",
                                               "pairing", "ble" };

void page_begin(void)
{
    up = false; which = PAGE_TOOL; left_s = 0; cause[0] = 0; log_new = false;
    lamp = false; lamp_s = 0; hilite_row = -1; hilite_s = 0; note_open = -1; notes_shown = 0; forget_s = 0;
    memset(&mk, 0, sizeof mk);
}

void page_lamp(const char *why)
{
    if (!up) { up = true; which = PAGE_TOOL; left_s = PAGE_S; }
    lamp = true; lamp_s = LAMP_S;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    snprintf(logline, sizeof logline, "page: the lamp (%s) -- the glass white for %.0f s, or until a tap", why, LAMP_S);
    log_new = true;
}

bool page_lamp_on(void) { return up && lamp; }

static void say_up(const char *why, bool again)
{
    snprintf(logline, sizeof logline, "page: %s (%s) for %.0f s -- %s; a tap or a drag down closes it, a drag up or a swipe turns it",
             again ? "up again" : "up", why, PAGE_S, PAGE_NAMES[which]);
    log_new = true;
}

void page_show(const char *why)
{
    bool was = up;
    up = true; left_s = PAGE_S; which = PAGE_TOOL; note_open = -1;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    say_up(why, was);
}

void page_show_which(int w, const char *why)
{
    bool was = up;
    up = true; left_s = PAGE_S; which = (w >= 0 && w < PAGE_N) ? w : PAGE_TOOL; note_open = -1;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    say_up(why, was);
}

/* THE NOTICE OPENED: a drag up or a swipe on an open notice is the way
 * back to the list, not the next page; the stay starts over */
static bool note_back(const char *why)
{
    if (which != PAGE_NOTICES_PAGE || note_open < 0) return false;
    note_open = -1;
    left_s = PAGE_S;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    snprintf(logline, sizeof logline, "page: the notice closed (%s) -- the notices, %.0f s again", why, PAGE_S);
    log_new = true;
    return true;
}

void page_next(const char *why)
{
    if (!up) { page_show(why); return; }
    if (note_back(why)) return;
    which = (which + 1) % PAGE_N;
    left_s = PAGE_S;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    snprintf(logline, sizeof logline, "page: turned (%s) -- %s, %.0f s again", why, PAGE_NAMES[which], PAGE_S);
    log_new = true;
}

/* THE SWIPES (3A): the ring backwards -- a swipe toward his right */
void page_prev(const char *why)
{
    if (!up) { page_show(why); return; }
    if (note_back(why)) return;
    which = (which + PAGE_N - 1) % PAGE_N;
    left_s = PAGE_S;
    strncpy(cause, why, sizeof cause - 1); cause[sizeof cause - 1] = 0;
    snprintf(logline, sizeof logline, "page: turned back (%s) -- %s, %.0f s again", why, PAGE_NAMES[which], PAGE_S);
    log_new = true;
}

void page_hide(const char *why)
{
    if (!up) return;
    bool was_lamp = lamp;
    up = false; left_s = 0; lamp = false; lamp_s = 0; hilite_row = -1; hilite_s = 0; note_open = -1; forget_s = 0;
    snprintf(logline, sizeof logline, "page: down (%s)%s", why, was_lamp ? " -- the lamp out" : "");
    log_new = true;
}

bool page_up(void) { return up; }
int  page_which(void) { return up ? which : -1; }
const char *page_name(int w) { return (w >= 0 && w < PAGE_N) ? PAGE_NAMES[w] : "down"; }
float page_left_s(void) { return up ? left_s : 0; }
int  page_note_open(void) { return up && which == PAGE_NOTICES_PAGE ? note_open : -1; }
/* THE NOTICE CLEARED FROM THE WRIST (T17): the hold on the open card -- the card closes to the list, the row answered */
int  page_note_hold(void)
{
    if (!up || lamp || which != PAGE_NOTICES_PAGE || note_open < 0) return -1;
    int row = note_open;
    note_open = -1;
    left_s = PAGE_S;
    snprintf(logline, sizeof logline, "page: a hold on notice %d -- cleared from the wrist; the notices, %.0f s again", row + 1, PAGE_S);
    log_new = true;
    return row;
}
void page_hold(void) { if (up && !lamp) left_s = PAGE_S; }   /* under the lamp the 8 s do not run anyway */
bool page_forget_armed(void) { return up && which == PAGE_SETTINGS6 && forget_s > 0; }

/* --- THE SETTINGS PAGE: the rows' places and the tap ----------------------- */
static bool is_settings(int w) { return w >= PAGE_SETTINGS && w < PAGE_SETTINGS + PAGE_SETTINGS_N; }
static int set_first_row(int w) { return SET_PAGE_FIRST[w - PAGE_SETTINGS]; }
static int set_rows_on(int w) { return SET_PAGE_ROWS[w - PAGE_SETTINGS]; }
static int set_row_y(int i) { return SET_Y0 + i * SET_ROW_H; }   /* i: the row's place on its page */

int page_tap(int x, int y)
{
    if (!up || lamp) return PAGE_ACT_NONE;
    if (which == PAGE_NOTICES_PAGE) {
        /* THE NOTICE OPENED: open, a tap anywhere on the disc keeps it (he is reading, as ABOUT); on the list, a
         * tap on a row that was drawn opens it; off the rows, the close as ever (NO NOTICES has no rows) */
        int dx = x - W / 2, dy = y - H / 2;
        if (dx * dx + dy * dy >= CLEAR_R * CLEAR_R) return PAGE_ACT_NONE;
        if (note_open >= 0) {
            left_s = PAGE_S;
            snprintf(logline, sizeof logline, "page: a tap on the open notice -- %.0f s again", PAGE_S);
            log_new = true;
            return PAGE_NOTE_0 + note_open;
        }
        for (int i = 0; i < notes_shown && i < PAGE_NOTICES; i++) {
            int ry = NOTE_Y0 + i * NOTE_ROW_H;
            if (y < ry - NOTE_TAP_PAD || y >= ry + NOTE_ROW_H - NOTE_TAP_PAD) continue;
            int half = half_at(ry, 5 * PX);
            if (x < W / 2 - half || x >= W / 2 + half) continue;
            note_open = i;
            left_s = PAGE_S;                                 /* the page's 8 s start over: he is reading */
            snprintf(logline, sizeof logline, "page: a tap on notice %d -- opened; %.0f s again (a drag up: the list)", i + 1, PAGE_S);
            log_new = true;
            return PAGE_NOTE_0 + i;
        }
        return PAGE_ACT_NONE;
    }
    if (which == PAGE_ABOUT) {
        /* ABOUT: read-only lines, no bands -- a tap anywhere on the disc keeps it 8 s more (he is reading) */
        int dx = x - W / 2, dy = y - H / 2;
        if (dx * dx + dy * dy >= CLEAR_R * CLEAR_R) return PAGE_ACT_NONE;
        left_s = PAGE_S;
        snprintf(logline, sizeof logline, "page: a tap on about -- %.0f s again", PAGE_S);
        log_new = true;
        return SET_ABOUT;
    }
    if (which == PAGE_BLE) {
        /* THE BLUETOOTH PAGE: the PAIRING line is a row (the window's tap); anywhere else on the disc keeps the page (he is reading, as ABOUT) */
        int dx = x - W / 2, dy = y - H / 2;
        if (dx * dx + dy * dy >= CLEAR_R * CLEAR_R) return PAGE_ACT_NONE;
        int ry = BLE_Y0 + BLE_PAIR_ROW * BLE_ROW_H;
        left_s = PAGE_S;
        if (y >= ry - SET_ROW_PAD && y < ry + 5 * PX + SET_ROW_PAD + 1) {
            hilite_row = SET_PAIRING; hilite_s = SET_HILITE_S;
            snprintf(logline, sizeof logline, "page: a tap on PAIRING (bluetooth) -- the window's; %.0f s again", PAGE_S);
            log_new = true;
            return SET_PAIRING;
        }
        snprintf(logline, sizeof logline, "page: a tap on bluetooth -- %.0f s again", PAGE_S);
        log_new = true;
        return SET_BLE;
    }
    if (!is_settings(which)) return PAGE_ACT_NONE;
    int first = set_first_row(which), n = set_rows_on(which);
    for (int i = 0; i < n; i++) {
        int ry = set_row_y(i);
        if (y < ry - SET_ROW_PAD || y >= ry + 5 * PX + SET_ROW_PAD + 1) continue;
        int half = half_at(ry, 5 * PX);
        if (x < W / 2 - half || x >= W / 2 + half) continue;
        hilite_row = first + i; hilite_s = SET_HILITE_S;
        left_s = PAGE_S;                                     /* the page's 8 s start over: he is using it */
        if (hilite_row == SET_FORGET && forget_s <= 0) {
            /* SETTINGS 6's FORGET: the first tap only arms it -- SURE? on the row for FORGET_ARM_S; main has nothing to do */
            forget_s = FORGET_ARM_S;
            snprintf(logline, sizeof logline, "page: a tap on FORGET -- SURE? a second tap within %.0f s forgets the phone; %.0f s again", FORGET_ARM_S, PAGE_S);
            log_new = true;
            return PAGE_ACT_KEEP;
        }
        if (hilite_row == SET_FORGET) forget_s = 0;          /* the second tap: the row's, for real */
        snprintf(logline, sizeof logline, "page: a tap on %s (%s) -- the row's; %.0f s again", SET_LABELS[hilite_row], PAGE_NAMES[which], PAGE_S);
        log_new = true;
        return hilite_row;
    }
    return PAGE_ACT_NONE;
}
const char *page_setting_name(int row) { return (row >= 0 && row < SET_N) ? SET_NAMES[row] : NULL; }
const char *page_setting_label(int row) { return (row >= 0 && row < SET_N) ? SET_LABELS[row] : NULL; }
int page_setting_by_name(const char *name)
{
    if (!name) return -1;
    if (!strcmp(name, "bluetooth")) return SET_BT;      /* `set bluetooth on|off` reads as the row does; `bt` is the short form */
    for (int i = 0; i < SET_N; i++) if (!strcmp(SET_NAMES[i], name)) return i;
    return -1;
}

const char *page_tick(float dt)
{
    t_s += dt > 0 ? dt : 0;
    if (t_s > 3600.0f) t_s -= 3600.0f;
    if (up && lamp) {
        /* THE LAMP: its own seconds; the page's wait */
        lamp_s -= dt;
        if (lamp_s <= 0) page_hide("the lamp's 20 s are up");
    } else if (up) {
        left_s -= dt;
        if (left_s <= 0) page_hide("its 8 s are up");
    }
    if (hilite_s > 0) { hilite_s -= dt; if (hilite_s <= 0) { hilite_s = 0; hilite_row = -1; } }
    if (forget_s > 0) {                                      /* SETTINGS 6's FORGET: the arm runs out, or drops when its page is gone */
        if (!up || which != PAGE_SETTINGS6) forget_s = 0;
        else { forget_s -= dt; if (forget_s <= 0) { forget_s = 0; snprintf(logline, sizeof logline, "page: FORGET disarmed -- the %.0f s passed without a second tap", FORGET_ARM_S); log_new = true; } }
    }
    if (!log_new) return NULL;
    log_new = false;
    return logline;
}

/* --- the pages ---------------------------------------------------------------- */
typedef struct { uint16_t bright, dim, shade, body; } inks_t;

/* the cell's row: the drop and the percent -- the number in the shade
 * when lean, breathing on the dock (BATTERY MANNERS), the shine else */
static void cell_row(uint16_t *fb, int cx, int y0, const page_in_t *in, const inks_t *k)
{
    char s[12];
    if (in->batt_pct >= 0) snprintf(s, sizeof s, "%d%%", in->batt_pct > 100 ? 100 : in->batt_pct);
    else snprintf(s, sizeof s, "--");
    uint16_t c = k->bright;
    if (in->cell_lean) c = k->shade;
    else if (in->charging) {
        float ph = 0.5f + 0.5f * sinf(2.0f * (float)M_PI * t_s / CELL_BREATH_S);
        c = ink(&theme_pal()->shine, CELL_BREATH_LOW + (1.0f - CELL_BREATH_LOW) * ph);
    }
    mark_text(fb, cx, y0, DROP, in->charging ? k->bright : k->body, s, c);
}

static void draw_tool(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    char s[24];
    /* the link: one dot at the top */
    {
        static const gel_rgba_t green = { 0.45f, 0.90f, 0.55f, 1.0f };   /* the shine pushed green: a door is there */
        uint16_t c = in->link == PAGE_LINK_FINE ? ink(&green, 1.0f) : k->shade;
        mark5(fb, W / 2 - 5, DOT_CY - 5, 2, DOT, c);
    }
    /* the hour, big */
    if (in->minute >= 0) hour_fmt(s, sizeof s, in->minute, in->h12, true);   /* 24H `07:41`; 12H `7:41 PM` (the page has the room) */
    else snprintf(s, sizeof s, "--:--");
    text_centred(fb, W / 2, HOUR_Y, PX_HOUR, s, k->bright);
    /* the date -- or the tool's row while a count runs */
    if (in->tool != PAGE_TOOL_NONE) {
        char t[12]; mss(t, sizeof t, in->tool_ms, in->tool == PAGE_TOOL_TIMER);
        if (in->tool == PAGE_TOOL_STOPWATCH && in->laps > 0) snprintf(s, sizeof s, "%s L%d", t, in->laps);
        else snprintf(s, sizeof s, "%s", t);
        mark_text(fb, W / 2, DATE_Y, in->tool == PAGE_TOOL_STOPWATCH ? WATCH : GLASS, k->body, s, k->bright);
    } else {
        if (in->wday >= 0 && in->wday < 7 && in->mon >= 0 && in->mon < 12)
            snprintf(s, sizeof s, "%s %d %s", DAYS[in->wday], in->mday, MONTHS[in->mon]);
        else snprintf(s, sizeof s, "-- -- ---");
        text_centred(fb, W / 2, DATE_Y, PX, s, k->dim);
    }
    /* the steps, the cell */
    snprintf(s, sizeof s, "%lu", (unsigned long)in->steps);
    mark_text(fb, ROW1_LX, ROW1_Y, FOOT, k->body, s, k->bright);
    cell_row(fb, ROW1_RX, ROW1_Y, in, k);
    /* the next alarm */
    if (in->alarm_min >= 0) {
        hour_fmt(s, sizeof s, in->alarm_min, in->h12, true);
        mark_text(fb, W / 2, ROW2_Y, BELL, k->body, s, k->bright);
    } else mark_text(fb, W / 2, ROW2_Y, BELL, k->shade, "--", k->shade);
    /* the laps' last two splits while there are any, else the sky's word */
    if (in->tool == PAGE_TOOL_STOPWATCH && in->laps > 0 && in->lap_ms) {
        char a[12], b[12];
        if (in->laps >= 2) {
            mss(a, sizeof a, in->lap_ms[in->laps - 2], false); mss(b, sizeof b, in->lap_ms[in->laps - 1], false);
            snprintf(s, sizeof s, "%s %s", a, b);
        } else mss(s, sizeof s, in->lap_ms[0], false);
        text_centred(fb, W / 2, ROW3_Y, PX, s, k->dim);
    } else if (in->sky && in->sky[0]) {
        int i = 0;
        for (; in->sky[i] && i < (int)sizeof s - 1; i++) s[i] = in->sky[i];
        s[i] = 0;
        text_centred(fb, W / 2, ROW3_Y, PX, s, k->dim);
    }
}

static void draw_week(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    char s[24];
    /* the night's hours: a moon and H:MM, "--" when none is known */
    if (in->night_min > 0) snprintf(s, sizeof s, "%d:%02d", in->night_min / 60, in->night_min % 60);
    else snprintf(s, sizeof s, "--");
    mark_text(fb, W / 2, WEEK_NIGHT_Y, MOON, in->night_min > 0 ? k->body : k->shade, s, in->night_min > 0 ? k->bright : k->shade);
    /* the seven rows: the week's best is the longest bar */
    uint32_t best = 0;
    for (int i = 0; i < 7; i++) if (in->week[i] != PAGE_NO_DAY && in->week[i] > best) best = in->week[i];
    for (int i = 0; i < 7; i++) {
        int y = WEEK_Y0 + i * WEEK_ROW_H;
        int wd = in->wday >= 0 ? ((in->wday - (6 - i)) % 7 + 7) % 7 : -1;
        bool today = i == 6;
        int half = half_at(y, 5 * PX);
        int x0 = W / 2 - half;
        text(fb, x0, y, PX, wd >= 0 ? DAYS[wd] : "---", today ? k->bright : k->dim);
        if (in->week[i] == PAGE_NO_DAY) { text(fb, W / 2 + half - text_w("--", PX), y, PX, "--", k->shade); continue; }   /* no file that day: the dashes where the number would be, no bar */
        int bar = best ? (int)((float)WEEK_BAR_MAX * (float)in->week[i] / (float)best + 0.5f) : 0;
        if (in->week[i] && bar < 1) bar = 1;
        rect(fb, x0 + WEEK_BAR_X - 34, y + PX, bar, 3 * PX, today ? k->bright : k->body);
        snprintf(s, sizeof s, "%lu", (unsigned long)in->week[i]);
        text(fb, W / 2 + half - text_w(s, PX), y, PX, s, today ? k->bright : k->dim);
    }
    /* the cell */
    cell_row(fb, W / 2, WEEK_CELL_Y, in, k);
}

/* the notice's name on the glass: the ledger's display name when it has
 * one, else the id's last dotted word; at most ``max`` glyphs; "?" for nothing */
static void note_name(char *s, int cap, const page_notice_t *nt, int max)
{
    const char *src = nt->name && nt->name[0] ? nt->name : NULL;
    if (!src) {
        const char *app = nt->app ? nt->app : "";
        const char *tail = strrchr(app, '.');
        src = tail ? tail + 1 : app;
    }
    int j = 0;
    for (; src[j] && j < max && j < cap - 1; j++) s[j] = src[j];
    s[j] = 0;
    if (!s[0]) snprintf(s, cap, "?");
}

/* THE NOTICE OPENED: a word-wrap -- the text laid into rows of the
 * page's chord from y0 at the pitch, PAGE_NOTE_ROWS at most; a word
 * wider than its row is cut there; the rows drawn, returned */
static int wrap_rows(uint16_t *fb, int y0, int pitch, int px, const char *text_in, uint16_t c)
{
    int rows = 0;
    const char *p = text_in;
    while (*p == ' ') p++;
    while (*p && rows < PAGE_NOTE_ROWS) {
        int y = y0 + rows * pitch;
        int max_px = 2 * half_at(y, 5 * px);
        int max_n = (max_px + px) / (4 * px);                /* glyphs that fit: (n * 4 - 1) * px <= max_px */
        if (max_n < 1) break;
        char row[64];
        int n = 0;
        /* whole words while they fit; the first word is cut if it alone does not */
        while (*p) {
            const char *e = p;
            while (*e && *e != ' ') e++;
            int wl = (int)(e - p);
            if (n == 0 && wl > max_n) { wl = max_n; e = p + wl; }                    /* one long word: cut at the chord */
            if (n + (n ? 1 : 0) + wl > max_n || n + (n ? 1 : 0) + wl >= (int)sizeof row) break;
            if (n) row[n++] = ' ';
            memcpy(row + n, p, (size_t)wl); n += wl;
            p = e;
            while (*p == ' ') p++;
        }
        row[n] = 0;
        if (!n) break;
        text(fb, W / 2 - text_w(row, px) / 2, y, px, row, c);
        rows++;
    }
    return rows;
}

/* THE NOTICE OPENED: the one notice, as a card inside the round -- the
 * name at the top in the shine, the title under it dimmed (centred,
 * cut to the chord), the message wrapped over the rows, the age at the
 * bottom; no title and no message = the name and the age alone */
static void draw_notice_open(uint16_t *fb, const page_notice_t *nt, const inks_t *k)
{
    char s[48];
    note_name(s, sizeof s, nt, 20);
    cut_to(s, 2 * half_at(OPEN_NAME_Y, 5 * PX), PX);
    text_centred(fb, W / 2, OPEN_NAME_Y, PX, s, k->bright);
    if (nt->title && nt->title[0]) {
        strncpy(s, nt->title, sizeof s - 1); s[sizeof s - 1] = 0;
        cut_to(s, 2 * half_at(OPEN_TITLE_Y, 5 * PX), PX);
        text_centred(fb, W / 2, OPEN_TITLE_Y, PX, s, k->dim);
    }
    if (nt->msg && nt->msg[0]) wrap_rows(fb, OPEN_MSG_Y0, OPEN_MSG_H, PX, nt->msg, k->bright);
    char age[12]; ago(age, sizeof age, nt->age_s);
    snprintf(s, sizeof s, "%s AGO", age);
    text_centred(fb, W / 2, OPEN_AGE_Y, PX, s, k->dim);
}

static void draw_notices(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    char s[48];
    if (in->notices <= 0 || !in->notice) {
        notes_shown = 0; note_open = -1;
        text_centred(fb, W / 2, H / 2 - (5 * PX) / 2, PX, "NO NOTICES", k->shade);
        return;
    }
    int n = in->notices > PAGE_NOTICES ? PAGE_NOTICES : in->notices;
    notes_shown = n;
    if (note_open >= n) note_open = n - 1;                   /* the ledger only grows, but the page is honest about what it has */
    if (note_open >= 0) { draw_notice_open(fb, &in->notice[note_open], k); return; }
    for (int i = 0; i < n; i++) {
        const page_notice_t *nt = &in->notice[i];
        int y = NOTE_Y0 + i * NOTE_ROW_H;
        int half = half_at(y, 5 * PX);
        /* the app's name (the ledger's, else the id's last word), uppercase, NOTE_APP_MAX glyphs; the age at the right */
        note_name(s, sizeof s, nt, NOTE_APP_MAX);
        char age[8]; ago(age, sizeof age, nt->age_s);
        text(fb, W / 2 - half, y, PX, s, k->bright);
        text(fb, W / 2 + half - text_w(age, PX), y, PX, age, k->dim);
        /* the title -- or, with none, the message's start -- dimmed, cut to the row below */
        const char *line = nt->title && nt->title[0] ? nt->title : nt->msg && nt->msg[0] ? nt->msg : NULL;
        if (line) {
            int y2 = y + 6 * PX;
            int half2 = half_at(y2, 5 * PX);
            strncpy(s, line, sizeof s - 1); s[sizeof s - 1] = 0;
            cut_to(s, 2 * half2, PX);
            text(fb, W / 2 - half2, y2, PX, s, k->dim);
        }
    }
}

/* THE SETTINGS PAGE: a title, the rows (the label left, the state right --
 * ON in the shine, OFF in the shade, the alarm's hour, the SLIME row's
 * HERE in the shine / AWAY in the shade; LINK NOW has no state: it is a
 * thing to do), a tapped row's label in the shine with a bar under it;
 * page 2's status lines */
static void draw_settings(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    char s[24], t[16];
    snprintf(t, sizeof t, "SETTINGS %d", which - PAGE_SETTINGS + 1);
    text_centred(fb, W / 2, SET_TITLE_Y, PX, t, k->dim);
    int first = set_first_row(which), n = set_rows_on(which);
    for (int i = 0; i < n; i++) {
        int row = first + i, y = set_row_y(i);
        int half = half_at(y, 5 * PX);
        bool lit = row == hilite_row && hilite_s > 0;
        int lw = text(fb, W / 2 - half, y, PX, SET_LABELS[row], lit ? k->bright : k->dim);
        const char *v = NULL; uint16_t vc = k->bright;
        switch (row) {
        case SET_SLIME: v = in->set_here ? "HERE" : "AWAY"; vc = in->set_here ? k->bright : k->shade; break;   /* THE SLIME ROW: the house's word, read each frame */
        case SET_LEASE: v = in->set_lease ? "ON" : "OFF"; vc = in->set_lease ? k->bright : k->shade; break;
        case SET_QUIET: v = in->set_quiet ? "ON" : "OFF"; vc = in->set_quiet ? k->bright : k->shade; break;
        case SET_FACE:  v = in->set_face  ? "ON" : "OFF"; vc = in->set_face  ? k->bright : k->shade; break;
        case SET_LOCK:  v = in->set_lock  ? "ON" : "OFF"; vc = in->set_lock  ? k->bright : k->shade; break;
        case SET_LINK:  v = in->link_word && in->link_word[0] ? in->link_word : NULL; vc = k->dim; break;   /* the link's word beside the action (dim: a state, not a switch) */
        case SET_ALARM:
            if (in->alarm_min >= 0) { hour_fmt(s, sizeof s, in->alarm_min, in->h12, true); v = s; }
            else { v = "OFF"; vc = k->shade; }
            break;
        /* MORE SETTINGS (21:12) */
        case SET_WIFI:    v = in->wifi_hold ? "HOLD" : "AUTO"; vc = in->wifi_hold ? k->bright : k->dim; break;
        case SET_NETWORK:
            if (in->net_ssid && in->net_ssid[0]) { snprintf(s, sizeof s, "%s", in->net_ssid); v = s; vc = k->dim; }
            else if (in->radio_on) { v = "JOINING"; vc = k->dim; }
            else { v = "OFF"; vc = k->shade; }
            break;
        case SET_ADDRESS:
            if (in->net_ip && in->net_ip[0]) { snprintf(s, sizeof s, "%s", in->net_ip); v = s; vc = k->dim; }
            else { v = "--"; vc = k->shade; }
            break;
        case SET_BT:      /* BLUETOOTH (ruling 58): OFF in the shade; `ON --` dimmed (the switch on, nobody on); PHONE / DESK / BOTH in the shine (who is on);
                           * `42 S` in the shine = THE PAIRING WINDOW's count (2026-10-06); "--" in the shade = no word */
            v = in->bt_word && in->bt_word[0] ? in->bt_word : "--";
            vc = !strcmp(v, "--") || !strcmp(v, "OFF") ? k->shade : !strcmp(v, "ON --") ? k->dim : k->bright;
            break;
        case SET_BRIGHT:  v = in->bright <= 0 ? "LOW" : in->bright == 1 ? "MID" : "HIGH"; break;
        case SET_SPELL:   v = in->spell_short ? "SHORT" : "LONG"; break;
        case SET_BUMPS:   v = in->bumps_on ? "ON" : "OFF"; vc = in->bumps_on ? k->bright : k->shade; break;   /* THE BUMPS' switch (2026-09-23) */
        case SET_CLOCK:   v = in->h12 ? "12H" : "24H"; break;
        case SET_QFROM:   hour_fmt_whole(s, sizeof s, in->quiet_from, in->h12); v = s; vc = in->quiet_desk ? k->shade : k->bright; break;   /* the desk's grain ruling: the shade */
        case SET_QTO:     hour_fmt_whole(s, sizeof s, in->quiet_to, in->h12); v = s; vc = in->quiet_desk ? k->shade : k->bright; break;
        case SET_PULSE:   /* THE PULSE ROW (2026-10-03): `--` in the shade = no sensor; `STILL` in the shade = waiting for the wrist (2026-10-08); `24 S` in the shine = reading, the count;
                           * `68 BPM` in the shine = the last read; `NONE` in the shade = it found no clean pulse; nothing = never read (an action, as LAMP) */
            if (in->pulse_absent) { v = "--"; vc = k->shade; }
            else if (in->pulse_still) { v = "STILL"; vc = k->shade; }   /* THE STILL SPOT (2026-10-08): the LEDs wait for the wrist; the count comes when they light */
            else if (in->pulse_left_s > 0) { snprintf(s, sizeof s, "%d S", in->pulse_left_s); v = s; }
            else if (in->pulse_bpm > 0) { snprintf(s, sizeof s, "%d BPM", in->pulse_bpm); v = s; }
            else if (in->pulse_fail) { v = in->pulse_thin ? "THIN" : "NONE"; vc = k->shade; }   /* THE PERFUSION INDEX (2026-10-14): the strap's word where the read failed for it; one word, no number */
            break;
        /* SETTINGS 6, THE HIDDEN SWITCHES (T15, 2026-10-12): words only */
        case SET_PAUTO:    v = in->pauto_on ? "ON" : "OFF"; vc = in->pauto_on ? k->bright : k->shade; break;
        case SET_DOCKHAND: v = in->dockhand_on ? "ON" : "OFF"; vc = in->dockhand_on ? k->bright : k->shade; break;
        case SET_EARGEAR:  v = in->ear_gear == 80 ? "LOW" : in->ear_gear == 160 ? "MID" : "FULL"; vc = in->ear_gear == 80 || in->ear_gear == 160 ? k->bright : k->dim; break;   /* FULL dimmed: the gear off (the default) */
        case SET_FORGET:   /* armed: SURE? in the shine; else PHONE dimmed (a bond to drop) or NONE in the shade (nothing to forget) */
            if (forget_s > 0) v = "SURE?";
            else if (in->ble_bonded) { v = "PHONE"; vc = k->dim; }
            else { v = "NONE"; vc = k->shade; }
            break;
        default: break;                                      /* LAMP: no state */
        }
        if (v) {
            /* the value at the row's right, cut to what the chord leaves beside the label (a long SSID) */
            char vv[24]; snprintf(vv, sizeof vv, "%s", v);
            cut_to(vv, 2 * half - lw - 2 * PX, PX);
            if (vv[0]) text(fb, W / 2 + half - text_w(vv, PX), y, PX, vv, vc);
        }
        if (lit) rect(fb, W / 2 - half, y + 5 * PX + 3, 2 * half, 2, k->body);   /* the bar under the tapped row */
    }
}

/* ABOUT (MORE SETTINGS, 21:12): read-only, numbers allowed -- the build,
 * the ip, the heap free, the cell's mV, the uptime, the steps today, and
 * (2026-09-17) THE BOX's status in a word each (the holder, the words
 * waiting, the box's sky: `KAPTOP 2 CLEAR`); one line each at the week's
 * pitch, the label dimmed left and the number in the shine right, each
 * cut to its chord (the seventh row sits at y 190, inside the rings) */
static void draw_about(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    char s[32];
    text_centred(fb, W / 2, SET_TITLE_Y, PX, "ABOUT", k->dim);
    const char *labels[7] = { "BUILD", "IP", "HEAP", "CELL", "UP", "STEPS", "BOX" };
    for (int i = 0; i < 7; i++) {
        int y = ABOUT_Y0 + i * ABOUT_H;
        int half = half_at(y, 5 * PX);
        int lw = text(fb, W / 2 - half, y, PX, labels[i], k->dim);
        uint16_t vc = k->bright;
        switch (i) {
        case 0: snprintf(s, sizeof s, "%s", in->build && in->build[0] ? in->build : "--"); break;
        case 1: if (in->net_ip && in->net_ip[0]) snprintf(s, sizeof s, "%s", in->net_ip); else { snprintf(s, sizeof s, "--"); vc = k->shade; } break;
        case 2: snprintf(s, sizeof s, "%luK", (unsigned long)(in->heap_free / 1024u)); break;
        case 3: if (in->cell_mv) snprintf(s, sizeof s, "%uMV", (unsigned)in->cell_mv); else { snprintf(s, sizeof s, "--"); vc = k->shade; } break;
        case 4:
            if (in->up_s >= 3600) snprintf(s, sizeof s, "%luH%02luM", (unsigned long)(in->up_s / 3600u), (unsigned long)((in->up_s / 60u) % 60u));
            else if (in->up_s >= 60) snprintf(s, sizeof s, "%luM%02luS", (unsigned long)(in->up_s / 60u), (unsigned long)(in->up_s % 60u));
            else snprintf(s, sizeof s, "%luS", (unsigned long)in->up_s);
            break;
        case 5: snprintf(s, sizeof s, "%lu", (unsigned long)in->steps); break;
        default: if (in->box && in->box[0]) snprintf(s, sizeof s, "%s", in->box); else { snprintf(s, sizeof s, "--"); vc = k->shade; } break;   /* THE BOX'S STATUS */
        }
        cut_to(s, 2 * half - lw - 2 * PX, PX);
        if (s[0]) text(fb, W / 2 + half - text_w(s, PX), y, PX, s, vc);
    }
}

/* THE BLUETOOTH PAGE (T16): five lines in words, the label dimmed left and
 * the state right -- the shine for a thing that is there (NEAR, OPEN, ON,
 * SOME), the shade for its absence (NONE, SHUT, OFF, NO BOND), AWAY dimmed
 * (a bond, the phone out of reach); PAIRING's OPEN turns while the window
 * runs (no count: the BLUETOOTH row has it); a tapped PAIRING lit as a row */
static void draw_ble(uint16_t *fb, const page_in_t *in, const inks_t *k)
{
    static const char *const labels[5] = { "PHONE", "AIR DOOR", "EAR", "PAIRING", "NOTICES" };
    text_centred(fb, W / 2, SET_TITLE_Y, PX, "BLUETOOTH", k->dim);
    for (int i = 0; i < BLE_ROWS; i++) {
        int y = BLE_Y0 + i * BLE_ROW_H;
        int half = half_at(y, 5 * PX);
        bool lit = i == BLE_PAIR_ROW && hilite_row == SET_PAIRING && hilite_s > 0;
        int lw = text(fb, W / 2 - half, y, PX, labels[i], lit ? k->bright : k->dim);
        const char *v; uint16_t vc = k->bright;
        switch (i) {
        case 0:  v = in->ble_phone ? "NEAR" : in->ble_bonded ? "AWAY" : "NONE"; vc = in->ble_phone ? k->bright : in->ble_bonded ? k->dim : k->shade; break;
        case 1:  v = in->ble_desk ? "OPEN" : "SHUT"; vc = in->ble_desk ? k->bright : k->shade; break;
        case 2:  v = in->ble_ear ? "ON" : !in->ble_on ? "OFF" : "NO BOND"; vc = in->ble_ear ? k->bright : k->shade; break;
        case 3:  v = in->ble_pairing ? "OPEN" : "SHUT"; vc = in->ble_pairing ? k->bright : k->shade; break;
        default: v = in->ble_notices ? "SOME" : "NONE"; vc = in->ble_notices ? k->bright : k->shade; break;
        }
        char vv[12]; snprintf(vv, sizeof vv, "%s", v);
        cut_to(vv, 2 * half - lw - 2 * PX, PX);
        if (vv[0]) text(fb, W / 2 + half - text_w(vv, PX), y, PX, vv, vc);
        if (lit) rect(fb, W / 2 - half, y + 5 * PX + 3, 2 * half, 2, k->body);
    }
}

/* --- the draw --------------------------------------------------------------- */
void page_draw(uint16_t *fb, const page_in_t *in)
{
    if (!up) return;
    if (lamp) {
        /* THE LAMP: the whole glass white, the rings under it too */
        mark_val(&mk, 0x1A3Bu);
        mark_box(&mk, 0, 0, W, H);
        for (int y = 0; y < H; y++) {
            float dy = (float)y + 0.5f - (float)H / 2;
            if (dy <= -LAMP_R || dy >= LAMP_R) continue;
            float half = sqrtf((float)LAMP_R * LAMP_R - dy * dy);
            int x0 = (int)ceilf((float)W / 2 - half - 0.5f), x1 = (int)floorf((float)W / 2 + half - 0.5f) + 1;
            if (x0 < 0) x0 = 0;
            if (x1 > W) x1 = W;
            for (int x = x0; x < x1; x++) fb[y * W + x] = WHITE;
        }
        return;
    }
    /* the glass's other face: black inside the rings (the face's hour,
     * drop and plug are under here; the rings at the rim stay) */
    mark_val(&mk, 0x9A6Eu + (uint32_t)which + (hilite_row >= 0 && hilite_s > 0 ? 0x100u * (uint32_t)(hilite_row + 1) : 0)
             + (note_open >= 0 ? 0x10000u * (uint32_t)(note_open + 1) : 0) + (forget_s > 0 ? 0x1000000u : 0));   /* a lit row is another frame; so is an open notice, or FORGET armed */
    mark_box(&mk, W / 2 - CLEAR_R, H / 2 - CLEAR_R, 2 * CLEAR_R, 2 * CLEAR_R);
    for (int y = 0; y < H; y++) {
        float dy = (float)y + 0.5f - (float)H / 2;
        if (dy <= -CLEAR_R || dy >= CLEAR_R) continue;
        float half = sqrtf((float)CLEAR_R * CLEAR_R - dy * dy);
        int x0 = (int)ceilf((float)W / 2 - half - 0.5f), x1 = (int)floorf((float)W / 2 + half - 0.5f) + 1;
        if (x0 < 0) x0 = 0;
        if (x1 > W) x1 = W;
        if (x1 > x0) memset(fb + y * W + x0, 0, (size_t)(x1 - x0) * sizeof *fb);
    }
    const gel_rgba_t *sn = &theme_pal()->shine, *sh = &theme_pal()->shade, *bd = &theme_pal()->body;
    inks_t k = { ink(sn, 1.0f), ink(sn, DIM_K), ink(sh, 1.0f), ink(bd, 1.0f) };
    if (which == PAGE_WEEK) draw_week(fb, in, &k);
    else if (which == PAGE_NOTICES_PAGE) draw_notices(fb, in, &k);
    else if (which == PAGE_BLE) draw_ble(fb, in, &k);
    else if (is_settings(which)) draw_settings(fb, in, &k);
    else if (which == PAGE_ABOUT) draw_about(fb, in, &k);
    else draw_tool(fb, in, &k);
}

void page_status(char *out, int cap)
{
    if (up && lamp) snprintf(out, cap, "page: THE LAMP (%s), %.1f s left; then %s", cause, lamp_s, PAGE_NAMES[which]);
    else if (up && which == PAGE_NOTICES_PAGE && note_open >= 0) snprintf(out, cap, "page: up -- the notices, notice %d open (%s), %.1f s left", note_open + 1, cause, left_s);
    else if (up) snprintf(out, cap, "page: up -- %s (%s), %.1f s left%s", PAGE_NAMES[which], cause, left_s, page_forget_armed() ? " -- FORGET armed" : "");
    else snprintf(out, cap, "page: down (a drag up the glass, or `page`, shows it for %.0f s; a drag up while it is up turns to the next: the tool page, his week, the notices, bluetooth, settings 1/6 .. 6/6, about)", PAGE_S);
}
