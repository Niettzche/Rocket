#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SENSOR_MPU6050 = 0,
    SENSOR_BMP180 = 1,
    SENSOR_NEO6M = 2,
    SENSOR_COUNT = 3
} SensorType;

const char *sensor_name(SensorType sensor);
bool sensor_from_name(const char *name, SensorType *out);

extern const SensorType SENSOR_LIST[SENSOR_COUNT];

#endif /* SENSORS_H */
