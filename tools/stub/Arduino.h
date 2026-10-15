#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
extern uint32_t fake_ms;
static inline uint32_t millis(void) { return fake_ms; }
struct FakeSerial { int printf(const char *f, ...) { va_list a; va_start(a, f); int r = vprintf(f, a); va_end(a); return r; } };
extern FakeSerial Serial;
