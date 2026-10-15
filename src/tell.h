/* THE TELL (Khalid, 2026-09-09: "make it a true home for the slime and
 * something I can use too, as a watch"): three small things the body
 * does with the day log's own facts, wordless (design/watch-daylog.md
 * section 4: the slime SHOWS, it never scolds -- never a number, never a
 * nag; design-zoom-out: the answer is in the material).
 *
 *   THE GLANCE THAT ANSWERS  on a wake by a raise (or a knock's arrival)
 *       while HOLDING, the gel's FIRST SECOND says the freshest true
 *       thing about him, once per fact, then it is the ordinary creature:
 *       a restless / short night -> droopy lids and a slow, low settle;
 *       a long sit still open -> the stretch urge (tall, eyes wide, a
 *       shake-off wobble); a good walk -> a contented bob; a sound night,
 *       the first raise of the morning -> the morning face (happy, one
 *       bright settle). The FRESHEST fact first; each fact tells ONCE
 *       (a told-id per fact, kept in RAM across naps).
 *   THE STEP BOB  when the step counter counts steps the gel bobs with
 *       the cadence -- one squash pulse per step -- for the first 10 s of
 *       a walk, then it rests and only the breath quickens (rest.cpp):
 *       the desk's run band, then the anti-Sonic law. Never asleep or
 *       on an empty glass. And THE WALK AS A LUMP (design/
 *       watch-inspiration.md C.1, "steps are the wrist's food"): when
 *       a walk of >= WALK_LUMP_STEPS ends, the gel carries a small lump
 *       (WALK_LUMP) that sinks away over WALK_LUMP_S -- he ate the walk.
 *       Nothing said, nothing counted on the glass.
 *   THE NIGHT LIGHT  asleep and a raise in the quiet hours: the dimmest
 *       glass, the Zs, the hour in the SHADE colour only, for 3 s, then
 *       dark again without waiting for the idle timers. Not a pet, no
 *       senses.
 *   UNDER THE WEATHER  (design/watch-ideas.md D34, 2026-10-08) a resting
 *       heart standing over the week's norm (nightsum.c, off the day
 *       files' hr bytes) wears as the restless night does: the droopy
 *       first face, the slow settle, once that day, after any fresher
 *       fact -- never a word, never a number (the log line has them).
 *   THE YAWN AT DUSK  (design/watch-ideas.md A2, 2026-09-23) in the hour
 *       before the quiet hours, once an evening, on a lit raise while
 *       HOLDING: the sleep bar for a beat, a tall intake, then a slow
 *       slump, the ordinary face after -- a body that knows its night is
 *       coming. Never over an untold fact (the fact goes first; the yawn
 *       waits for the next raise in the hour), never dozing, never in
 *       the dark house (a raise lights it) or on the shelf (a raise ends
 *       a shelf run before the tell sees the cue). Its id is the date.
 *
 * No Arduino here: the time, the facts and the steps are parameters
 * (main.cpp reads them from rest.cpp / daylog.cpp / house.cpp / face.cpp
 * and fills tell_in_t once a frame); tools/tell_test.cpp runs it on the
 * laptop. The outputs are a squash to add, an eye kind to wear, a kick
 * or two for the springs and the night light's flags -- the gel, the
 * bowl and the fall are untouched. */
#pragma once
#include <stdint.h>
#include <stdbool.h>

enum { TELL_CUE_NONE = 0, TELL_CUE_RAISE = 1, TELL_CUE_ARRIVAL = 2 };
enum { TELL_NONE = 0, TELL_NIGHT, TELL_SIT, TELL_WALK, TELL_MORNING, TELL_MOOD, TELL_YAWN, TELL_WEATHER, TELL_ARRIVE };   /* TELL_MOOD: THE GRAIN's carried mood, when there is no fact; TELL_YAWN: the yawn at dusk, before the mood's face; TELL_ARRIVE: THE OTHER HOUSE'S DAY on the arrival's first face (C21) */
/* HE ARRIVES WITH THE OTHER HOUSE'S DAY (design/watch-ideas.md C21, 2026-10-09): the grain line's `sat=` (minutes he has sat at
 * the desk this session) and `pets=` (pets today there), fresh off a live line -- the ARRIVAL's first face is the desk's day: heavy
 * lids for a long session, content after a petted afternoon; the wrist's own untold fact waits for the next raise */
#define ARRIVE_SAT_MIN  90      /* a desk session this long lands him heavy-lidded (the wrist's own sit tells at 60: the desk's chair is softer) */
#define ARRIVE_SAT_S    1.5f    /* the heavy lids' length at ARRIVE_SAT_MIN (the droopy tell's own 1.5 s), scaled by the session... */
#define ARRIVE_SAT_X    2.0f    /* ...up to this many times (a three-hour sit: 3 s of lids; never longer -- fins are death) */
#define ARRIVE_PETS     5       /* pets today at the desk from which he lands content */
#define ARRIVE_PETS_S   1.2f    /* the content face's length, with one soft settle */
/* THE YAWN AT DUSK's window: the hour before the quiet hours begin --
 * main.cpp fills tell_in_t.dusk from rest.cpp's quiet hours as
 * !rest_night_at(minute) && rest_night_at(minute + TELL_DUSK_BEFORE_MIN) */
#define TELL_DUSK_BEFORE_MIN 60

typedef struct {
    float    dt;
    int      cue;               /* TELL_CUE_*: this frame's wake by a raise, or the knock's arrival (main clears it) */
    bool     holding;           /* house_holding(): the gel is drawn */
    bool     lit;               /* the screen is on */
    bool     asleep;            /* rest: the deep sleep (no tell, no bob) */
    bool     dozing;            /* rest: asleep or stirring (the night light) */
    bool     night;             /* rest_night_now(): the quiet hours */
    bool     dusk;              /* THE YAWN AT DUSK: the hour before the quiet hours (TELL_DUSK_BEFORE_MIN); false with no clock */
    int      minute;            /* the local minute of the day, -1 = no clock (the log's hh:mm) */
    uint32_t date;              /* YYYYMMDD, 0 = no clock (the norm's once-a-day id) */
    uint32_t rows;              /* rest_rows(): the minute clock the facts are dated by */
    bool     span_known;        /* rest_last_span(): the fields below hold */
    int      span_min, span_end_minute, span_awakenings, span_restless_min;
    uint32_t span_end_row;
    bool     span_night;
    int      sit_min;           /* rest_sit_min() */
    int      steps_hour;        /* rest_steps_last(60) */
    int      rows_since_walk;   /* rest_rows_since_walk(): -1 = none in the hour */
    uint32_t steps_today;       /* wrist_steps_today() */
    uint32_t norm;              /* face_norm() */
    uint32_t step_seq;          /* wrist_step_seq(): steps counted, cumulative */
    /* UNDER THE WEATHER (nightsum.h, watch-ideas D34, 2026-10-08): nightsum_weather() -- today's resting
     * median and the week's norm, bpm, BOTH non-zero only when today stands WEATHER_BPM over the week
     * (main fills them so; 0 / 0 = nothing to wear). The fact is the day's: told once per date. */
    int      weather_hr, weather_norm;
    int      desk_sat_min;      /* THE OTHER HOUSE'S DAY: the grain line's sat= (0 = none / stale; link_grain_day) */
    int      desk_pets;         /* ... and its pets= */
} tell_in_t;

typedef struct {
    float  squash;              /* added to the authored squash this frame (the tell's curve, the bob's pulse) */
    int    eye_kind;            /* GEL_EYE_* to wear, or -1 = the ordinary face */
    float  kick1, kick2;        /* one-shot kicks for the springs (lateral, vertical), this frame only */
    bool   night_light;         /* the dimmest glass, the hour in the shade, the sleep look kept */
    bool   night_done;          /* one-shot: the light's seconds are up -- go dark now */
    bool   log_new;             /* a log line waiting (tell_log_taken) */
    char   log[112];
} tell_out_t;

void tell_begin(void);
const tell_out_t *tell_tick(const tell_in_t *in);   /* one call a frame */
void tell_log_taken(void);
/* `tell` over serial: what it would say now (the freshest untold fact
 * and its cause), what it has said, the walk and the light */
void tell_status(char *out, int cap);
int  tell_active(void);                 /* TELL_* playing now, for the test */
/* THE NIGHT AFTER A REBOOT (daylog.cpp + store.told_night): the night
 * fact's told-id is a row number, which a reboot resets -- so daylog.cpp
 * reads which span row was told (0 = none) to stamp the night's stable
 * id into NVS, and after the files' replay marks the rebuilt night told
 * when the id matches; the sit and the walk stay RAM (their ids are
 * hours and days: told again once, at worst) */
uint32_t tell_night_told_row(void);
void     tell_told_night(uint32_t span_end_row);
bool tell_walking(void);                /* the bob's walk is on (steps within the gap) */
/* the walk's lump, 0..1 (WALK_LUMP at the walk's end, sinking over
 * WALK_LUMP_S): main.cpp takes the larger of this and gesture_lump()
 * for gel_cell_params_t.lump -- one belly */
float tell_lump(void);
