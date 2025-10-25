#ifndef SUMMARIES_H
#define SUMMARIES_H

#include "activity_tracker.h"
#include "sensor_workers.h"

void log_start_summary(SensorCaps caps);
void log_final_summary(const ActivityTracker *tracker);

#endif /* SUMMARIES_H */
