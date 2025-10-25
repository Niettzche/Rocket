#include "sensors.h"

#include <string.h>

typedef struct {
    SensorType type;
    const char *name;
} SensorName;

static const SensorName SENSOR_NAMES[] = {
    {SENSOR_MPU6050, "mpu6050"},
    {SENSOR_BMP180, "bmp180"},
    {SENSOR_NEO6M, "neo6m"},
};

const SensorType SENSOR_LIST[SENSOR_COUNT] = {
    SENSOR_MPU6050,
    SENSOR_BMP180,
    SENSOR_NEO6M,
};

const char *sensor_name(SensorType sensor) {
    for (size_t i = 0; i < sizeof(SENSOR_NAMES) / sizeof(SENSOR_NAMES[0]); ++i) {
        if (SENSOR_NAMES[i].type == sensor) {
            return SENSOR_NAMES[i].name;
        }
    }
    return "unknown";
}

bool sensor_from_name(const char *name, SensorType *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof(SENSOR_NAMES) / sizeof(SENSOR_NAMES[0]); ++i) {
        if (strcmp(name, SENSOR_NAMES[i].name) == 0) {
            *out = SENSOR_NAMES[i].type;
            return true;
        }
    }
    return false;
}
