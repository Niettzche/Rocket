#ifndef AGGREGATOR_H
#define AGGREGATOR_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdbool.h>

#include "activity_tracker.h"
#include "message_queue.h"
#include "sensor_message.h"

typedef bool (*payload_sender_fn)(const char *json_payload);

typedef struct {
    SensorQueue *queue;
    ActivityTracker *tracker;
    const SensorType *expected_sensors;
    size_t sensor_count;
    payload_sender_fn send_payload;
    double emit_interval_seconds;
    volatile sig_atomic_t *stop_flag;
} AggregatorConfig;

int aggregator_start(pthread_t *thread, const AggregatorConfig *config);

#endif /* AGGREGATOR_H */
