#ifndef SENSOR_WORKERS_H
#define SENSOR_WORKERS_H

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#include "message_queue.h"
#include "sensors.h"

typedef struct {
    bool has_mpu;
    bool has_bmp;
    bool has_gps;
} SensorCaps;

SensorCaps sensor_caps_get(void);
int sensor_threads_start(SensorQueue *queue, volatile sig_atomic_t *stop_flag, pthread_t threads[SENSOR_COUNT]);

#endif /* SENSOR_WORKERS_H */
