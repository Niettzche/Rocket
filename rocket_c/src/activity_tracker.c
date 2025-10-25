#include "activity_tracker.h"

#include <string.h>

void activity_tracker_init(ActivityTracker *tracker) {
    if (!tracker) {
        return;
    }
    memset(tracker, 0, sizeof(ActivityTracker));
}

void activity_tracker_update(ActivityTracker *tracker, SensorType sensor, bool is_dummy) {
    if (!tracker || sensor < 0 || sensor >= SENSOR_COUNT) {
        return;
    }
    tracker->seen[sensor] = true;
    tracker->last_dummy[sensor] = is_dummy;
}

bool activity_tracker_seen(const ActivityTracker *tracker, SensorType sensor) {
    if (!tracker || sensor < 0 || sensor >= SENSOR_COUNT) {
        return false;
    }
    return tracker->seen[sensor];
}

bool activity_tracker_last_dummy(const ActivityTracker *tracker, SensorType sensor) {
    if (!tracker || sensor < 0 || sensor >= SENSOR_COUNT) {
        return false;
    }
    return tracker->last_dummy[sensor];
}

bool activity_tracker_zero_sent(const ActivityTracker *tracker) {
    if (!tracker) {
        return false;
    }
    return tracker->zero_signal_sent;
}

void activity_tracker_record_zero_signal(ActivityTracker *tracker, double timestamp, double magnitude) {
    if (!tracker) {
        return;
    }
    if (tracker->zero_signal_sent) {
        return;
    }
    tracker->zero_signal_sent = true;
    tracker->zero_signal_timestamp = timestamp;
    tracker->zero_signal_magnitude = magnitude;
}

ZeroSignalInfo activity_tracker_zero_details(const ActivityTracker *tracker) {
    ZeroSignalInfo info = {0};
    if (!tracker) {
        return info;
    }
    info.sent = tracker->zero_signal_sent;
    info.timestamp = tracker->zero_signal_timestamp;
    info.magnitude = tracker->zero_signal_magnitude;
    return info;
}
