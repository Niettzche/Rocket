#include "logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *const LEVEL_STRINGS[] = {
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_SYS] = "SYS",
    [LOG_LEVEL_PAYLOAD] = "PAYLOAD",
};

static const char *const LEVEL_COLORS[] = {
    [LOG_LEVEL_INFO] = "\033[92m",
    [LOG_LEVEL_WARN] = "\033[93m",
    [LOG_LEVEL_ERROR] = "\033[91m",
    [LOG_LEVEL_DEBUG] = "\033[94m",
    [LOG_LEVEL_SYS] = "\033[96m",
    [LOG_LEVEL_PAYLOAD] = "\033[95m",
};

static const char *const COLOR_RESET = "\033[0m";

void logger_init(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

void log_message(LogLevel level, const char *sensor, const char *fmt, ...) {
    if (level < LOG_LEVEL_INFO || level > LOG_LEVEL_PAYLOAD) {
        level = LOG_LEVEL_INFO;
    }

    pthread_mutex_lock(&logger_mutex);

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    const char *lvl = LEVEL_STRINGS[level];
    const char *color = LEVEL_COLORS[level];
    if (color == NULL) {
        color = LEVEL_COLORS[LOG_LEVEL_INFO];
    }
    if (sensor == NULL) {
        sensor = "SYSTEM";
    }

    FILE *stream = (level == LOG_LEVEL_ERROR) ? stderr : stdout;
    fprintf(stream, "%s[%s] [%s] %s%s\n", color, lvl, sensor, COLOR_RESET, buffer);
    fflush(stream);

    pthread_mutex_unlock(&logger_mutex);
}

void log_payload(const char *payload) {
    if (payload == NULL) {
        return;
    }
    pthread_mutex_lock(&logger_mutex);
    fprintf(stdout, "%s[PAYLOAD] [AGREGADOR]%s fotito uwu\n%s\n",
            LEVEL_COLORS[LOG_LEVEL_PAYLOAD], COLOR_RESET, payload);
    fflush(stdout);
    pthread_mutex_unlock(&logger_mutex);
}
