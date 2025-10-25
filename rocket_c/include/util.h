#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

double current_time_seconds(void);
void sleep_seconds(double seconds);
void isoformat_utc(double timestamp, char *buffer, size_t buffer_len);

#endif /* UTIL_H */
