#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "sensor_message.h"

typedef struct SensorQueue SensorQueue;

SensorQueue *sensor_queue_create(size_t capacity);
void sensor_queue_destroy(SensorQueue *queue);
void sensor_queue_close(SensorQueue *queue);

bool sensor_queue_push(SensorQueue *queue, const SensorMessage *message);
bool sensor_queue_pop(SensorQueue *queue, SensorMessage *out, double timeout_seconds);

#endif /* MESSAGE_QUEUE_H */
