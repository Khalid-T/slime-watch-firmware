/* THE RADIO BUDGET's decision on the laptop (radio.cpp has no Arduino in it):
 *   c++ -std=gnu++17 -I tools/stub -I src -o /tmp/radio_test tools/radio_test.cpp src/radio.cpp && /tmp/radio_test
 * A wake 2 min after a link with nothing pending -> no dial; a whistle
 * -> dial; 11 min of awake time -> dial (the claim); 31 min of wall
 * time with the glass barely lit -> dial (the courtesy); UNKNOWN ->
 * dial at once; the dock -> never; nothing to dial with -> never (and
 * the house's offline clock runs); never linked since boot -> dial;
 * forced -> dial; the "next due" arithmetic. THE LIT GLASS POLLS: the
 * lease verb's cadence over an open session (5 s lit, the minute dim)
 * and when the session is held open (lit, the heap not low, a verb to
 * poll with). THE CABLE NEVER CLAIMS: the truth table -- on the cable
 * and EMPTY no claim (and no hold of a lit session); on the cable and
 * HOLDING the beat; off the cable and EMPTY the claim; the house never
 * told asks once, cable or not; the hold's `come` is not the rule's
 * (it never asks it). THE SHELF NEVER CLAIMS (2026-09-17): the same
 * table with worn = false -- EMPTY and still sends no claim at all,
 * HOLDING and still keeps the beat, UNKNOWN still asks once. THE LAST
 * DROP'S MANNERS (2026-10-12): under the spent line off the cable
 * nothing dials -- not a whistle, not the day, not his hand, not the
 * house never told; the reason is not "wanted" (no offline clock); on
 * the cable the rule stands as ever; the dock still outranks it. */
#include "radio.h"
#include "house.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int checks = 0;
static radio_in_t base(void)
{
    radio_in_t in; memset(&in, 0, sizeof in);
    in.now_ms = 1000u * 3600u;              /* an hour up */
    in.have_ok = true;
    in.last_ok_ms = in.now_ms - 120u * 1000u;   /* the last link 2 min ago */
    in.awake_since_ok_s = 20;               /* one glance of it awake */
    in.house_state = HOUSE_HOLDING;
    in.can = true;
    return in;
}
static void is(const radio_in_t *in, int want, const char *what)
{
    int r = radio_reason(in);
    printf("  %-58s -> %s%s\n", what, radio_reason_name(r), r == want ? "" : "   <-- WRONG");
    assert(r == want);
    checks++;
}

int main(void)
{
    assert(LINK_EVERY_S == 600 && LINK_STALE_S == 1800 && LINK_LINGER_S == 5);
    puts("-- a wake 2 min after a link, nothing pending: the glass lights, no radio");
    { radio_in_t in = base(); is(&in, RADIO_REST, "2 min wall, 20 s awake, holding, nothing to send"); assert(!radio_wanted(RADIO_REST)); checks++;
      assert(radio_next_due_s(&in) == 580); checks++; }
    puts("-- the same wake, EMPTY (the desk has him): still resting");
    { radio_in_t in = base(); in.house_state = HOUSE_EMPTY; is(&in, RADIO_REST, "2 min, empty"); }
    puts("-- a whistle (or a sense worth the radio, or a closed day): dial");
    { radio_in_t in = base(); in.pending = true; is(&in, RADIO_PENDING, "pending"); assert(radio_wanted(RADIO_PENDING)); checks++; }
    puts("-- 11 min of AWAKE time since the last ok: the claim is due");
    { radio_in_t in = base(); in.awake_since_ok_s = 11 * 60; in.last_ok_ms = in.now_ms - 11u * 60u * 1000u; is(&in, RADIO_EVERY, "11 min awake"); }
    puts("-- 599 s awake: not yet; 600: due");
    { radio_in_t in = base(); in.awake_since_ok_s = 599; is(&in, RADIO_REST, "599 s awake"); in.awake_since_ok_s = 600; is(&in, RADIO_EVERY, "600 s awake");
      in.awake_since_ok_s = 599; assert(radio_next_due_s(&in) == 1); checks++; }
    puts("-- 11 min of WALL time with one glance of it awake: resting (the courtesy is at 30)");
    { radio_in_t in = base(); in.last_ok_ms = in.now_ms - 11u * 60u * 1000u; is(&in, RADIO_REST, "11 min wall, 20 s awake"); assert(radio_next_due_s(&in) == 580); checks++; }
    puts("-- 31 min of wall time, the glass barely lit: the box has heard nothing -> the courtesy dial");
    { radio_in_t in = base(); in.last_ok_ms = in.now_ms - 31u * 60u * 1000u; is(&in, RADIO_STALE, "31 min wall, 20 s awake");
      in.last_ok_ms = in.now_ms - 29u * 60u * 1000u; is(&in, RADIO_REST, "29 min wall"); assert(radio_next_due_s(&in) == 60); checks++; }
    puts("-- the wall clock wrapped (millis past 49 days): the subtraction still counts forward");
    { radio_in_t in = base(); in.now_ms = 1000; in.last_ok_ms = 0xffffffffu - 200u * 1000u; is(&in, RADIO_REST, "201 s across the wrap");
      in.last_ok_ms = 0xffffffffu - 1900u * 1000u; is(&in, RADIO_STALE, "1901 s across the wrap"); }
    puts("-- the house never told (a first boot): dial at once, whatever the clocks");
    { radio_in_t in = base(); in.house_state = HOUSE_UNKNOWN; is(&in, RADIO_UNKNOWN, "unknown, 2 min"); }
    puts("-- never linked since boot: the first claim, at once");
    { radio_in_t in = base(); in.have_ok = false; in.awake_since_ok_s = 5; is(&in, RADIO_EVERY, "no ok yet"); assert(radio_next_due_s(&in) == 0); checks++; }
    puts("-- the dock: never a radio dial, whatever else stands");
    { radio_in_t in = base(); in.docked = true; in.pending = true; in.house_state = HOUSE_UNKNOWN; in.have_ok = false; in.forced = true;
      is(&in, RADIO_DOCKED, "docked + pending + unknown + never + forced"); assert(!radio_wanted(RADIO_DOCKED)); checks++; }
    puts("-- nothing to dial with (no network / door / key): the radio stays off, but the house counts it as offline");
    { radio_in_t in = base(); in.can = false; in.pending = true; in.house_state = HOUSE_UNKNOWN; is(&in, RADIO_NOWAY, "no way + pending + unknown"); assert(radio_wanted(RADIO_NOWAY)); checks++; }
    puts("-- ...but his hand (a hold, `link now`) outranks a missing door or key: provisioning joins first");
    { radio_in_t in = base(); in.can = false; in.forced = true; is(&in, RADIO_FORCED, "no way + forced"); }
    puts("-- forced (`link now`, a hold): dial, over the clocks");
    { radio_in_t in = base(); in.forced = true; is(&in, RADIO_FORCED, "forced, 2 min, holding"); }
    puts("-- the order of reasons: pending outranks the clocks, unknown outranks pending");
    { radio_in_t in = base(); in.pending = true; in.awake_since_ok_s = 700; in.last_ok_ms = in.now_ms - 3000u * 1000u; is(&in, RADIO_PENDING, "pending + every + stale");
      in.house_state = HOUSE_UNKNOWN; is(&in, RADIO_UNKNOWN, "unknown + pending + every + stale"); }
    puts("-- every reason has a name");
    for (int r = RADIO_REST; r <= RADIO_SPENT; r++) { assert(strcmp(radio_reason_name(r), "?") != 0); checks++; }
    assert(!strcmp(radio_reason_name(99), "?")); checks++;
    puts("-- THE LIT GLASS POLLS: the lease verb's cadence over an open session");
    assert(LINK_BEAT_S == 60 && CLAIM_LIT_S == 5 && BEAT_LIT_S == 5); checks++;
    assert(radio_lease_every_s(true, HOUSE_EMPTY) == CLAIM_LIT_S); checks++;     /* lit and empty: the claim every 5 s (a `go` lands within it) */
    assert(radio_lease_every_s(true, HOUSE_UNKNOWN) == CLAIM_LIT_S); checks++;   /* never told: the same claim decides */
    assert(radio_lease_every_s(true, HOUSE_HOLDING) == BEAT_LIT_S); checks++;    /* lit and holding: the beat every 5 s (a desk `come` sinks him within it) */
    assert(radio_lease_every_s(false, HOUSE_EMPTY) == LINK_BEAT_S); checks++;    /* the glass dim: the minute, as before */
    assert(radio_lease_every_s(false, HOUSE_HOLDING) == LINK_BEAT_S); checks++;
    puts("-- ...and the session is held open only while lit, never under the heap floor, and only with a verb to poll with");
    assert(radio_hold_session(true, false, true)); checks++;
    assert(!radio_hold_session(false, false, true)); checks++;                    /* dimmed: the linger applies, `bye` follows */
    assert(!radio_hold_session(true, true, true)); checks++;                      /* a TLS session that left under 60 KB: not held */
    assert(!radio_hold_session(true, false, false)); checks++;                    /* lit, the heap fine, but nothing to poll with (the cable, EMPTY): not held */
    puts("-- THE CABLE NEVER CLAIMS: the truth table (worn throughout)");
    assert(!radio_may_claim(true, HOUSE_EMPTY, true)); checks++;                  /* on the cable, the house EMPTY: the honest claim is off */
    assert(radio_lease_verb(true, HOUSE_EMPTY, true) == LEASE_NONE); checks++;    /*   ...so the cadence has no verb: the linger counts, the lit session is not held */
    assert(!radio_hold_session(true, false, radio_lease_verb(true, HOUSE_EMPTY, true) != LEASE_NONE)); checks++;
    assert(radio_lease_verb(true, HOUSE_HOLDING, true) == LEASE_BEAT); checks++;  /* on the cable, HOLDING: the beat goes (a held lease stays honest) */
    assert(!radio_may_claim(true, HOUSE_HOLDING, true)); checks++;                /*   ...but the re-claim a `no lapsed` asks for does not: the lapse is let go */
    assert(radio_hold_session(true, false, radio_lease_verb(true, HOUSE_HOLDING, true) != LEASE_NONE)); checks++;   /* the lit session is held for the beat (a desk `come` sinks him within 5 s) */
    assert(radio_may_claim(false, HOUSE_EMPTY, true)); checks++;                  /* off the cable, worn, EMPTY: the claim, as ever */
    assert(radio_lease_verb(false, HOUSE_EMPTY, true) == LEASE_CLAIM); checks++;
    assert(radio_lease_verb(false, HOUSE_HOLDING, true) == LEASE_BEAT); checks++;
    assert(radio_may_claim(true, HOUSE_UNKNOWN, true) && radio_may_claim(false, HOUSE_UNKNOWN, true)); checks++;   /* never told: the one first question, cable or not (else "the house never told" dials in a loop at the console) */
    assert(radio_lease_verb(true, HOUSE_UNKNOWN, true) == LEASE_CLAIM); checks++;
    puts("-- THE SHELF NEVER CLAIMS (grasp 0.4, the worn half; the box arbitrates on `claim worn` since 09-17): still = no claim, the beat still goes");
    assert(!radio_may_claim(false, HOUSE_EMPTY, false)); checks++;                /* off the cable but STILL (the shelf): no honest claim -- a `claim still` on an empty lease would land */
    assert(radio_lease_verb(false, HOUSE_EMPTY, false) == LEASE_NONE); checks++;  /*   ...the cadence has no verb: the lit session is not held, the linger counts */
    assert(!radio_hold_session(true, false, radio_lease_verb(false, HOUSE_EMPTY, false) != LEASE_NONE)); checks++;
    assert(radio_lease_verb(false, HOUSE_HOLDING, false) == LEASE_BEAT); checks++; /* HOLDING and still: the beat (carrying `still` -- the box may hand him to a desk that claims; the wrist never lets go by itself) */
    assert(!radio_may_claim(false, HOUSE_HOLDING, false)); checks++;              /*   ...the re-claim a `no lapsed` asks for stays unsent on the shelf */
    assert(radio_may_claim(false, HOUSE_UNKNOWN, false) && radio_may_claim(true, HOUSE_UNKNOWN, false)); checks++;   /* never told: the one first question even on the shelf (it carries `still`: the box's own rule decides) */
    assert(radio_lease_verb(false, HOUSE_UNKNOWN, false) == LEASE_CLAIM); checks++;
    assert(radio_may_claim(false, HOUSE_EMPTY, true) && !radio_may_claim(false, HOUSE_EMPTY, false)); checks++;   /* the same wrist, EMPTY, off the cable: worn claims, still does not -- the whole of ruling 50 */
    assert(radio_lease_every_s(true, HOUSE_EMPTY) == CLAIM_LIT_S); checks++;      /* the cadence's number is untouched by the cable: the verb is what goes missing */
    /* the hold on the cable (`come`, the knocks, the desk's `free` knock, the rising's claim) never asks this rule:
     * link.cpp's whistle path is ahead of the cadence in the schedule and reads none of these -- there is no
     * pure function to refuse them, and that is the proof */
    puts("-- THE LAN KNOCK's line: `lost` / `free`, bare or `<what> <stamp> <mac>`; anything else is not a knock");
    { knock_t k;
      assert(radio_knock_parse("lost", &k) && !strcmp(k.what, "lost") && !k.signed_); checks++;
      assert(radio_knock_parse("free\r\n", &k) && !strcmp(k.what, "free") && !k.signed_); checks++;
      assert(radio_knock_parse("lost 29815630 0123456789ABCDEF0123456789abcdef\n", &k) && k.signed_ && k.stamp == 29815630u
             && !strcmp(k.mac, "0123456789abcdef0123456789abcdef")); checks++;             /* the mac read lower-case, the stamp a number */
      assert(!radio_knock_parse("eat", &k)); checks++;                                        /* not a knock */
      assert(!radio_knock_parse("lost x", &k)); checks++;                                     /* a stamp that is not a number */
      assert(!radio_knock_parse("lost 29815630 0123", &k)); checks++;                         /* a short mac */
      assert(!radio_knock_parse("lost 29815630 0123456789abcdef0123456789abcdef zz", &k)); checks++;   /* trailing junk */
      assert(!radio_knock_parse("", &k) && !radio_knock_parse(NULL, &k)); checks++; }
    puts("-- ...and a stamp is fresh within a minute of the wrist's clock, either way");
    assert(radio_knock_fresh(100, 100) && radio_knock_fresh(100, 101) && radio_knock_fresh(101, 100)); checks++;
    assert(!radio_knock_fresh(100, 102) && !radio_knock_fresh(102, 100)); checks++;
    puts("-- THE KNOCK (LAN or the dock): the verdict is one function, whichever door the line came in by");
    { knock_t bare, sig; char why[24];
      assert(radio_knock_parse("free", &bare) && radio_knock_parse("free 100 0123456789abcdef0123456789abcdef", &sig));
      const char *ours = "0123456789abcdef0123456789abcdef", *theirs = "ffffffffffffffffffffffffffffffff";
      assert(radio_knock_judge(&bare, false, NULL, true, 100, why, sizeof why) == 0 && !strcmp(why, "unsigned")); checks++;   /* no key here: a bare knock taken, said as such */
      assert(radio_knock_judge(&sig, false, NULL, true, 100, why, sizeof why) == 0 && !strcmp(why, "unsigned")); checks++;    /* ... a signed one too (nothing to check it with) */
      assert(radio_knock_judge(&bare, true, NULL, true, 100, why, sizeof why) == 1 && !strcmp(why, "no key")); checks++;      /* a key remembered: unsigned is refused */
      assert(radio_knock_judge(&sig, true, theirs, true, 100, why, sizeof why) == 1 && !strcmp(why, "no key")); checks++;     /* mis-signed: refused */
      assert(radio_knock_judge(&sig, true, ours, true, 100, why, sizeof why) == 0 && !strcmp(why, "ok")); checks++;           /* signed, fresh: taken */
      assert(radio_knock_judge(&sig, true, ours, true, 101, why, sizeof why) == 0); checks++;                                  /* a minute off: still fresh */
      assert(radio_knock_judge(&sig, true, ours, true, 102, why, sizeof why) == 1 && !strcmp(why, "stale")); checks++;        /* two off: stale */
      assert(radio_knock_judge(&sig, true, ours, false, 9999, why, sizeof why) == 0); checks++;                                /* no clock: the mac is checked, the stamp not */
      assert(radio_knock_judge(&sig, true, theirs, false, 100, why, sizeof why) == 1 && !strcmp(why, "no key")); checks++; }  /* no clock does not excuse a wrong mac */
    puts("-- BATTERY MANNERS: the cell under 10 % (lean) doubles the EVERY clock; the courtesy and the news stay");
    { radio_in_t in = base(); in.lean = true; in.awake_since_ok_s = 600; is(&in, RADIO_REST, "lean, 600 s awake: not yet");
      in.awake_since_ok_s = 1199; is(&in, RADIO_REST, "lean, 1199 s awake: not yet");
      in.awake_since_ok_s = 1200; is(&in, RADIO_EVERY, "lean, 1200 s awake: the claim");
      in.awake_since_ok_s = 599; assert(radio_next_due_s(&in) == 601 && radio_every_s(&in) == 1200); checks++;
      in.awake_since_ok_s = 20; in.last_ok_ms = in.now_ms - 31u * 60u * 1000u; is(&in, RADIO_STALE, "lean, 31 min of wall: the courtesy as ever");
      in.last_ok_ms = in.now_ms - 120u * 1000u; in.pending = true; is(&in, RADIO_PENDING, "lean, something to send: dial as ever");
      in.pending = false; in.lean = false; in.awake_since_ok_s = 600; is(&in, RADIO_EVERY, "not lean again: 600 s is the claim"); }

    puts("-- THE LAST DROP'S MANNERS: under the spent line, off the cable, the radio refuses every WiFi dial");
    { radio_in_t in = base(); in.spent = true; is(&in, RADIO_SPENT, "spent, resting anyway: spent");
      assert(!radio_wanted(RADIO_SPENT)); checks++;                                  /* not offline: it waits */
      in.pending = true; is(&in, RADIO_SPENT, "spent, a whistle / the day to sync: waits for the cable");
      in.pending = false; in.awake_since_ok_s = 700; is(&in, RADIO_SPENT, "spent, the claim due: waits");
      in.awake_since_ok_s = 20; in.last_ok_ms = in.now_ms - 31u * 60u * 1000u; is(&in, RADIO_SPENT, "spent, the courtesy due: waits");
      in.last_ok_ms = in.now_ms - 120u * 1000u; in.forced = true; is(&in, RADIO_SPENT, "spent, his hand (`link now` / a hold): refused too (the burst kills the body)");
      in.forced = false; in.house_state = HOUSE_UNKNOWN; is(&in, RADIO_SPENT, "spent, the house never told: waits");
      in.house_state = HOUSE_HOLDING; in.have_ok = false; is(&in, RADIO_SPENT, "spent, never linked since boot: waits");
      in.have_ok = true; in.can = false; is(&in, RADIO_SPENT, "spent and nothing to dial with: spent says it first (no offline clock)");
      in.can = true; in.docked = true; is(&in, RADIO_DOCKED, "spent on the dock: the dock (the cable is the link)");
      in.docked = false; in.cable = true; in.pending = true; is(&in, RADIO_PENDING, "spent ON THE CABLE, a whistle: dials (the rail is fed)");
      in.pending = false; in.awake_since_ok_s = 700; is(&in, RADIO_EVERY, "spent on the cable, the claim due: dials as ever");
      in.awake_since_ok_s = 20; is(&in, RADIO_REST, "spent on the cable, nothing due: resting");
      in.cable = false; in.spent = false; in.pending = true; is(&in, RADIO_PENDING, "off the line again: the whistle dials");
      assert(!strcmp(radio_reason_name(RADIO_SPENT), "spent: the radio waits for the cable")); checks++; }

    printf("ALL GOOD (%d checks)\n", checks);
    return 0;
}
