/* THE HOUSE's state machine on the laptop (no board; Arduino stubbed by tools/stub/Arduino.h):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/house_test tools/house_test.cpp src/house.cpp && /tmp/house_test
 * A first boot (nothing in NVS) draws empty; the senses gated while
 * empty; the offline-worn rule (90 s awake with no link, worn = the
 * default home; still on a shelf = kept; linked = the clock never runs);
 * the honest claim's `no desk` = the departure; `no box` sticky; the
 * whistle's `ok held` = the arrival; a beat's `no lapsed` = one more
 * claim, then empty; `release`; the serial aid; THE NIGHT RELEASE (empty
 * at once with no theatre, the box's confirmation / refusal / `no desk`
 * / no answer, the rising's `ok held` as the ordinary arrival); a reboot
 * with NVS = holding drawn at once. The fx numbers of the rise and the
 * sink are printed. */
#include "Arduino.h"
#include "house.h"
#include "store.h"
#include <assert.h>
#include <string.h>
#include <math.h>
uint32_t fake_ms = 1000; FakeSerial Serial;
store_t store; int saves = 0;
void store_save_house(void) { saves++; printf("  [nvs] house=%d\n", store.house); }
bool clock_is_set(void) { return false; }
static void run(float s, bool linked, uint32_t still) { for (float t = 0; t < s; t += 0.025f) { house_tick(0.025f, linked, still); fake_ms += 25; } }
static void fx(const char *tag) { house_fx_t f; house_fx(&f); printf("  fx %s: gel %d squash %.2f lift %.0f eye %.2f gaze_y %d eyes-up %.0f\n", tag, f.gel, f.squash, f.lift, f.eye_k, f.gaze_y, f.eye_alpha); }
int main() {
    memset(&store, 0, sizeof store);
    puts("-- first boot: nothing in NVS");
    house_begin(); assert(house_state() == HOUSE_UNKNOWN); fx("boot"); { house_fx_t f; house_fx(&f); assert(!f.gel); }
    puts("-- the senses while empty");
    assert(house_sense_ok("wrist:raised") && house_sense_ok("wrist:slept") && house_sense_ok("wrist:shaken"));
    assert(!house_sense_ok("wrist:pet") && !house_sense_ok("wrist:water") && !house_sense_ok("wrist:moving"));
    puts("-- offline 60 s, worn: still empty");
    run(60, false, 1000); assert(house_state() == HOUSE_UNKNOWN);
    puts("-- offline 40 s more, but STILL (a shelf, 6 min): stays");
    run(40, false, 6 * 60000); assert(house_state() == HOUSE_UNKNOWN);
    puts("-- worn again: the default home");
    run(0.05f, false, 1000); assert(house_state() == HOUSE_HOLDING); assert(house_take_event() == HOUSE_EV_ARRIVED);
    fx("rise t=0"); { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == 0 && f.eye_alpha == 0); }   /* the arrival keeps its rise: no goodbye in it */
    run(0.3f, false, 1000); fx("rise t=0.3"); run(0.4f, false, 1000); fx("standing");
    { house_fx_t f; house_fx(&f); assert(f.gel && fabsf(f.squash) < 1e-6f && fabsf(f.lift) < 1e-6f && f.eye_k == 1.0f && f.gaze_y == 0); }
    assert(house_sense_ok("wrist:pet")); assert(store.house == HOUSE_HOLDING);
    puts("-- a door found: the honest claim says no desk -> THE GOODBYE (a look up, one settle over 0.6 s), then the departure (the silent correction)");
    run(5, true, 1000);
    assert(!house_lease("claim", "no desk")); assert(house_state() == HOUSE_EMPTY); assert(house_take_event() == HOUSE_EV_LEFT);
    assert(!house_take_sink());                                                   /* THE BUMPS: the goodbye is not the sink yet */
    fx("bye t=0"); { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == -1 && f.eye_alpha == 1.0f && f.eye_k == 1.0f && fabsf(f.squash) < 1e-6f && f.lift == 0); }   /* the trace's frame: the resting silhouette, the look up */
    run(0.3f, true, 1000); fx("bye t=0.3");
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == -1 && f.squash > 0.07f && f.squash <= 0.08f && f.lift == 0 && f.eye_k == 1.0f); }   /* the settle's peak, still standing */
    run(0.25f, true, 1000); fx("bye t=0.55");
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == -1 && f.squash < 0.03f && f.lift == 0); }
    assert(!house_take_sink());                                                   /* THE BUMPS: still the goodbye at 0.55 s */
    run(0.1f, true, 1000); fx("sink t=0.05");
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == 0 && f.eye_alpha == 0 && f.lift < 0 && f.eye_k < 1.0f); }   /* the goodbye said: the sink, eyes going */
    assert(house_take_sink()); assert(!house_take_sink());                        /* THE BUMPS: the sink began (the goodbye's settle done) -- once, then taken */
    puts("  the sink's frame taken once: the bump's moment");
    run(0.2f, true, 1000); fx("sink t=0.25"); run(0.3f, true, 1000); fx("gone");
    { house_fx_t f; house_fx(&f); assert(!f.gel); }
    puts("-- called back mid-goodbye: it stands again, no sink");
    house_lease("come", "ok held"); house_take_event(); run(0.2f, true, 1000);
    house_lease("claim", "no desk"); house_take_event(); run(0.05f, true, 1000);
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == 0 && f.lift < 0); }   /* told to leave mid-arrival: no one to face yet, the sink at once */
    assert(house_take_sink());                                                    /* THE BUMPS: the at-once sink is a sink too */
    run(0.6f, true, 1000); { house_fx_t f; house_fx(&f); assert(!f.gel); }
    house_lease("come", "ok held"); house_take_event(); run(0.7f, true, 1000);     /* the arrival done */
    house_lease("claim", "no desk"); house_take_event(); run(0.2f, true, 1000);
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == -1); }
    house_lease("come", "ok held"); assert(house_take_event() == HOUSE_EV_NONE); run(0.05f, true, 1000);
    { house_fx_t f; house_fx(&f); assert(f.gel && f.gaze_y == 0 && f.squash == 0 && f.lift == 0); }
    house_lease("claim", "no desk"); house_take_event(); run(1.2f, true, 1000);
    { house_fx_t f; house_fx(&f); assert(!f.gel); }
    puts("-- linked and empty: the offline clock must not run");
    run(200, true, 1000); assert(house_state() == HOUSE_EMPTY);
    puts("-- no box on a claim: sticky");
    assert(!house_lease("claim", "no box")); assert(house_state() == HOUSE_EMPTY);
    puts("-- the whistle: come -> ok held: the arrival");
    assert(!house_lease("come", "ok held")); assert(house_state() == HOUSE_HOLDING); assert(house_take_event() == HOUSE_EV_ARRIVED);
    run(1, true, 1000);
    puts("-- a beat's no lapsed while holding: claim again once, still holding");
    assert(house_lease("beat", "no lapsed") == true); assert(house_state() == HOUSE_HOLDING);
    puts("-- the claim's no lapsed: empty");
    assert(house_lease("claim", "no lapsed") == false); assert(house_state() == HOUSE_EMPTY); house_take_event(); run(1, true, 1000);
    puts("-- held again, then the second long press: release -> ok released: the departure");
    house_lease("claim", "ok held"); house_take_event(); run(1, true, 1000);
    house_lease("release", "ok released"); assert(house_state() == HOUSE_EMPTY && house_take_event() == HOUSE_EV_LEFT); run(1, true, 1000);
    puts("-- the serial aid");
    house_force(HOUSE_HOLDING, "serial"); assert(house_holding()); run(1, true, 1000);
    house_force(HOUSE_EMPTY, "serial"); assert(!house_holding()); assert(house_take_event() == HOUSE_EV_LEFT); run(1, true, 1000);
    char l[200]; house_status(l, sizeof l); printf("  %s\n", l);
    puts("-- THE NIGHT RELEASE (ruling 52): holding, the night begins -> empty at once, NO theatre (the glass is dark)");
    house_force(HOUSE_HOLDING, "serial"); house_take_event(); run(1, true, 1000);
    assert(house_take_sink());                                           /* THE BUMPS: the forced sink above began (taken here; the aid plays the theatre) */
    assert(house_night_release()); assert(house_state() == HOUSE_EMPTY && house_release_pending());
    { house_fx_t f; house_fx(&f); assert(!f.gel); }                      /* no goodbye, no sink: nothing drawn from this frame on */
    assert(house_take_event() == HOUSE_EV_NONE); assert(store.house == HOUSE_EMPTY);
    run(1, true, 1000); assert(!house_take_sink());                       /* THE BUMPS: the night's release never sinks, so it never bumps */
    house_status(l, sizeof l); printf("  %s\n", l); assert(strstr(l, "the night's") != NULL);
    puts("-- the box confirms (`ok released`): empty, nothing drawn");
    house_lease("release", "ok released"); assert(house_state() == HOUSE_EMPTY && !house_release_pending());
    { house_fx_t f; house_fx(&f); assert(!f.gel); }
    puts("-- the rising's claim: `ok held` = the ordinary arrival (the rise plays at the first lit frame -- the morning)");
    house_lease("claim", "ok held"); assert(house_holding() && house_take_event() == HOUSE_EV_ARRIVED);
    { house_fx_t f; house_fx(&f); assert(f.gel && f.lift < 0); } run(1, true, 1000);
    puts("-- the night's release refused (`no box`): holding again, no theatre either (no rise at a 3 am glance)");
    assert(house_night_release()); house_lease("release", "no box"); assert(house_holding() && !house_release_pending());
    { house_fx_t f; house_fx(&f); assert(f.gel && f.squash == 0 && f.lift == 0 && f.eye_k == 1.0f); }
    assert(house_take_event() == HOUSE_EV_NONE);
    puts("-- `no desk` to the night's release: empty (he is elsewhere either way)");
    assert(house_night_release()); house_lease("release", "no desk"); assert(house_state() == HOUSE_EMPTY);
    { house_fx_t f; house_fx(&f); assert(!f.gel); }
    puts("-- empty at the night's onset: nothing to let go of");
    assert(!house_night_release()); assert(house_state() == HOUSE_EMPTY);
    puts("-- no answer to the night's release: the wrist's word stands (empty)");
    house_force(HOUSE_HOLDING, "serial"); house_take_event(); run(1, true, 1000);
    assert(house_night_release()); house_release_unanswered(); assert(!house_release_pending() && house_state() == HOUSE_EMPTY);
    puts("-- a reboot with NVS = holding: drawn at once, no theatre");
    store.house = HOUSE_HOLDING; house_begin(); { house_fx_t f; house_fx(&f); assert(f.gel && f.squash == 0); }
    assert(house_take_event() == HOUSE_EV_NONE);
    printf("ALL GOOD (%d nvs saves)\n", saves);
}
