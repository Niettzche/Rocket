#ifndef SENSOR_MESSAGE_H
#define SENSOR_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "sensors.h"

typedef struct {
    double ax;
    double ay;
    double az;
    double gx;
    double gy;
    double gz;
    double pitch;
    double roll;
    double yaw;
    bool dummy;
} MpuData;

typedef struct {
    double temperature;
    double pressure;
    bool has_temperature;
    bool has_pressure;
    bool dummy;
    char raw[128];
    bool has_raw;
} BmpData;

typedef struct {
    double latitude;
    double longitude;
    double altitude;
    bool has_latitude;
    bool has_longitude;
    bool has_altitude;
    char fix_time[32];
    bool has_fix_time;
    char raw[128];
    bool has_raw;
    bool dummy;
} GpsData;

typedef struct {
    SensorType sensor;
    double timestamp;
    union {
        MpuData mpu;
        BmpData bmp;
        GpsData gps;
    } data;
} SensorMessage;

#endif /* SENSOR_MESSAGE_H */
