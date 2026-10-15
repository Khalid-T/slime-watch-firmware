/* The serial console: a line buffer (never a delay-based read: the
 * provisioning script sends a burst), double-quoted arguments with \"
 * inside them, every command in `help`, and the dock -- a `hi <nonce>`
 * from the desk's SerialLink hands the port to the link. */
#pragma once

void console_begin(void);
void console_tick(void);                /* from loop(): drains Serial */
