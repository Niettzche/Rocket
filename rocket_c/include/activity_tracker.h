#ifndef ACTIVITY_TRACKER_H
#define ACTIVITY_TRACKER_H

#include <stdbool.h>

#include "sensors.h"

typedef struct {
    bool seen[SENSOR_COUNT];
    bool last_dummy[SENSOR_COUNT];
    bool zero_signal_sent;
    double zero_signal_timestamp;
    double zero_signal_magnitude;
} ActivityTracker;

void activity_tracker_init(ActivityTracker *tracker);
void activity_tracker_update(ActivityTracker *tracker, SensorType sensor, bool is_dummy);
bool activity_tracker_zero_sent(const ActivityTracker *tracker);
void activity_tracker_record_zero_signal(ActivityTracker *tracker, double timestamp, double magnitude);
bool activity_tracker_seen(const ActivityTracker *tracker, SensorType sensor);
bool activity_tracker_last_dummy(const ActivityTracker *tracker, SensorType sensor);

typedef struct {
    bool sent;
    double timestamp;
    double magnitude;
} ZeroSignalInfo;

ZeroSignalInfo activity_tracker_zero_details(const ActivityTracker *tracker);

#endif /* ACTIVITY_TRACKER_H */
