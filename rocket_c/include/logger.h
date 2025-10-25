#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_SYS,
    LOG_LEVEL_PAYLOAD
} LogLevel;

void logger_init(void);
void log_message(LogLevel level, const char *sensor, const char *fmt, ...);
void log_payload(const char *payload);

#endif /* LOGGER_H */
