/* THE HTTP DOOR'S KNOCK (2026-10-13, THE OPERATOR NIGHT T11): which of
 * the wrist's HTTP paths take the knock's signature, pure.
 *
 * The page (`/`) and the face (`/screen.bmp`, `/screen.bin`) stay OPEN:
 * the face is not a secret and the page's <img> needs it. `/doctor`,
 * `/status.json`, `/day`, `/day/` and `/day/<file>` (GET) and `POST
 * /pet` are GATED: on a provisioned watch (a key remembered) they must
 * carry the knock's own signature in the query -- `?stamp=<utc
 * minute>&mac=<hex>`, mac = HMAC-SHA256(the watch key, "<stamp> http
 * <METHOD> <path>")'s first 32 hex, the path bare (no query) -- judged
 * by link_sign_check (link.cpp), which is radio_knock_judge over the
 * message: refused 403 `no key` (unsigned, or the mac is not the
 * desk's) or 403 `stale` (the stamp more than KNOCK_STALE_MIN 1 off the
 * wrist's set minute; an unset clock checks the mac alone). A bare
 * board (no key) answers as before, the first unsigned ask logged
 * `taken unsigned` and the rest not (httpgate_t.said_unsigned). `POST
 * /lease` and `POST /art` carry their own signatures (the knock's, the
 * art's) and are not gated here; an unknown path is not gated (it is a
 * 404 either way). The stale-minute rule is the knock's (radio.h
 * KNOCK_STALE_MIN), not repeated here.
 *
 * The query is cut off the path FIRST (httpgate_split): every route
 * then sees the bare path (`/doctor?stamp=..` matches `/doctor`), and
 * the gate reads the query. net.cpp's http_route makes one gate call;
 * the desk's watchlink.py signs its fetches with door_query (the same
 * message shape, test_watch.py's parity test). No heap, no state but
 * the one bool. */
#ifndef SLIME_HTTPGATE_H
#define SLIME_HTTPGATE_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HTTPGATE_WHAT_CAP 40            /* "http POST /day/20261013.bin" and room */

typedef struct {
    bool said_unsigned;                 /* a bare board's `taken unsigned` line, said once */
} httpgate_t;

/* Cut the query off a request path in place: "/doctor?stamp=1&mac=ab"
 * becomes "/doctor" and the query "stamp=1&mac=ab" is returned ("" when
 * there is none). */
static inline char *httpgate_split(char *path)
{
    char *q = strchr(path, '?');
    if (!q) return path + strlen(path);
    *q = 0;
    return q + 1;
}

/* THE TABLE: true for a path that takes the knock's signature. */
static inline bool httpgate_gated(bool post, const char *path)
{
    if (post) return false;   /* POST /pet stays OPEN (the operator's ruling, 2026-10-13 00:50): a stranger's pet is only a pet, and the share page's pet button is his phone's */
    return !strcmp(path, "/doctor") || !strcmp(path, "/status.json")
        || !strcmp(path, "/day") || !strncmp(path, "/day/", 5);
}

/* The signed message's tail: "http GET /doctor" / "http POST /pet"
 * (link_sign_check puts "<stamp> " before it). Cut to the cap. */
static inline void httpgate_what(bool post, const char *path, char *out, int cap)
{
    snprintf(out, (size_t)cap, "http %s %s", post ? "POST" : "GET", path);
}

/* The verdict over the knock's judge (rc + why from link_sign_check /
 * radio_knock_judge): 0 = let it through (*say set when a bare board's
 * `taken unsigned` line is due, the first time only); 1 = refuse, with
 * the status line and the one-word body to send. */
static inline int httpgate_verdict(httpgate_t *g, int rc, const char *why,
                                   const char **status, const char **body, bool *say)
{
    *say = false;
    if (rc == 0) {
        if (!strcmp(why, "unsigned") && !g->said_unsigned) { g->said_unsigned = true; *say = true; }
        *status = "200 OK"; *body = "";
        return 0;
    }
    *status = "403 Forbidden";
    *body = !strcmp(why, "stale") ? "stale\n" : "no key\n";
    return 1;
}

#endif
