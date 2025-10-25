#include "util.h"

#include <stdio.h>
#include <time.h>

double current_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void sleep_seconds(double seconds) {
    if (seconds <= 0.0) {
        return;
    }
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1e9);
    while (nanosleep(&req, &req) == -1) {
        continue;
    }
}

void isoformat_utc(double timestamp, char *buffer, size_t buffer_len) {
    if (buffer == NULL || buffer_len == 0) {
        return;
    }
    time_t seconds = (time_t)timestamp;
    double fractional = timestamp - (double)seconds;
    if (fractional < 0) {
        fractional = 0;
    }
    struct tm tm_utc;
    gmtime_r(&seconds, &tm_utc);
    long micros = (long)(fractional * 1e6 + 0.5);
    if (micros >= 1000000) {
        micros -= 1000000;
        seconds += 1;
        gmtime_r(&seconds, &tm_utc);
    }
    snprintf(
        buffer,
        buffer_len,
        "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
        tm_utc.tm_year + 1900,
        tm_utc.tm_mon + 1,
        tm_utc.tm_mday,
        tm_utc.tm_hour,
        tm_utc.tm_min,
        tm_utc.tm_sec,
        micros
    );
}
