#include "aggregator.h"

#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "activity_tracker.h"
#include "logger.h"
#include "util.h"

#define ZERO_ACCEL_REF 1.0
#define ZERO_ACCEL_TOLERANCE 0.05
#define ZERO_ACCEL_REQUIRED 2
#define ZERO_ACCEL_MIN_DELAY 1.0

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef struct {
    AggregatorConfig config;
    SensorMessage latest[SENSOR_COUNT];
    bool has_latest[SENSOR_COUNT];
    int zero_acc_count;
    double zero_last_detection;
} AggregatorState;

static void sb_init(StringBuilder *sb) {
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

static bool sb_reserve(StringBuilder *sb, size_t additional) {
    size_t required = sb->length + additional + 1;
    if (required <= sb->capacity) {
        return true;
    }
    size_t new_capacity = sb->capacity ? sb->capacity * 2 : 256;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    char *new_data = (char *)realloc(sb->data, new_capacity);
    if (!new_data) {
        return false;
    }
    sb->data = new_data;
    sb->capacity = new_capacity;
    return true;
}

static bool sb_append(StringBuilder *sb, const char *text) {
    if (!text) {
        return true;
    }
    size_t len = strlen(text);
    if (!sb_reserve(sb, len)) {
        return false;
    }
    memcpy(sb->data + sb->length, text, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
    return true;
}

static bool sb_append_format(StringBuilder *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return false;
    }
    if (!sb_reserve(sb, (size_t)needed)) {
        va_end(args);
        return false;
    }
    vsnprintf(sb->data + sb->length, sb->capacity - sb->length, fmt, args);
    sb->length += (size_t)needed;
    va_end(args);
    return true;
}

static bool sb_append_indent(StringBuilder *sb, int spaces) {
    if (spaces <= 0) {
        return true;
    }
    if (!sb_reserve(sb, (size_t)spaces)) {
        return false;
    }
    memset(sb->data + sb->length, ' ', (size_t)spaces);
    sb->length += (size_t)spaces;
    sb->data[sb->length] = '\0';
    return true;
}

static bool sb_append_json_string(StringBuilder *sb, const char *value) {
    if (!sb_append(sb, "\"")) {
        return false;
    }
    if (value) {
        for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
            unsigned char c = *p;
            switch (c) {
                case '\\':
                    if (!sb_append(sb, "\\\\")) return false;
                    break;
                case '"':
                    if (!sb_append(sb, "\\\"")) return false;
                    break;
                case '\n':
                    if (!sb_append(sb, "\\n")) return false;
                    break;
                case '\r':
                    if (!sb_append(sb, "\\r")) return false;
                    break;
                case '\t':
                    if (!sb_append(sb, "\\t")) return false;
                    break;
                default:
                    if (c < 0x20) {
                        if (!sb_append_format(sb, "\\u%04x", c)) return false;
                    } else {
                        if (!sb_reserve(sb, 1)) return false;
                        sb->data[sb->length++] = (char)c;
                        sb->data[sb->length] = '\0';
                    }
                    break;
            }
        }
    }
    return sb_append(sb, "\"");
}

static char *sb_finalize(StringBuilder *sb) {
    if (!sb->data) {
        sb_reserve(sb, 1);
        sb->data[0] = '\0';
    }
    return sb->data;
}


static bool append_mpu_payload(StringBuilder *sb, const SensorMessage *msg, int indent) {
    const MpuData *mpu = &msg->data.mpu;
    sb_append(sb, "{\n");
    sb_append_indent(sb, indent + 2);
    char ts[64];
    isoformat_utc(msg->timestamp, ts, sizeof(ts));
    sb_append(sb, "\"timestamp\": ");
    sb_append_json_string(sb, ts);
    sb_append(sb, ",\n");

    sb_append_indent(sb, indent + 2);
    sb_append(sb, "\"accel_g\": {\"x\": ");
    sb_append_format(sb, "%.4f, \"y\": %.4f, \"z\": %.4f}", mpu->ax, mpu->ay, mpu->az);
    sb_append(sb, ",\n");

    sb_append_indent(sb, indent + 2);
    sb_append(sb, "\"gyro_dps\": {\"x\": ");
    sb_append_format(sb, "%.3f, \"y\": %.3f, \"z\": %.3f}", mpu->gx, mpu->gy, mpu->gz);
    sb_append(sb, ",\n");

    sb_append_indent(sb, indent + 2);
    sb_append(sb, "\"attitude_deg\": {\"pitch\": ");
    sb_append_format(sb, "%.2f, \"roll\": %.2f, \"yaw\": %.2f}", mpu->pitch, mpu->roll, mpu->yaw);
    if (mpu->dummy) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"dummy\": true\n");
    } else {
        sb_append(sb, "\n");
    }
    sb_append_indent(sb, indent);
    sb_append(sb, "}");
    return true;
}

static bool append_bmp_payload(StringBuilder *sb, const SensorMessage *msg, int indent) {
    const BmpData *bmp = &msg->data.bmp;
    sb_append(sb, "{\n");
    sb_append_indent(sb, indent + 2);
    char ts[64];
    isoformat_utc(msg->timestamp, ts, sizeof(ts));
    sb_append(sb, "\"timestamp\": ");
    sb_append_json_string(sb, ts);
    if (bmp->has_raw) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"raw\": ");
        sb_append_json_string(sb, bmp->raw);
    } else if (bmp->has_temperature || bmp->has_pressure) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"raw\": {\"T\": ");
        if (bmp->has_temperature) {
            sb_append_format(sb, "%.2f", bmp->temperature);
        } else {
            sb_append(sb, "null");
        }
        sb_append(sb, ", \"P\": ");
        if (bmp->has_pressure) {
            sb_append_format(sb, "%.2f", bmp->pressure);
        } else {
            sb_append(sb, "null");
        }
        sb_append(sb, "}");
    }
    if (bmp->dummy) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"dummy\": true");
    }
    sb_append(sb, "\n");
    sb_append_indent(sb, indent);
    sb_append(sb, "}");
    return true;
}

static bool append_gps_payload(StringBuilder *sb, const SensorMessage *msg, int indent) {
    const GpsData *gps = &msg->data.gps;
    sb_append(sb, "{\n");
    sb_append_indent(sb, indent + 2);
    char ts[64];
    isoformat_utc(msg->timestamp, ts, sizeof(ts));
    sb_append(sb, "\"timestamp\": ");
    sb_append_json_string(sb, ts);
    if (gps->has_latitude) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append_format(sb, "\"latitude\": %.6f", gps->latitude);
    }
    if (gps->has_longitude) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append_format(sb, "\"longitude\": %.6f", gps->longitude);
    }
    if (gps->has_altitude) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append_format(sb, "\"altitude\": %.1f", gps->altitude);
    }
    if (gps->has_fix_time) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"fix_time\": ");
        sb_append_json_string(sb, gps->fix_time);
    }
    if (gps->has_raw) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"raw\": ");
        sb_append_json_string(sb, gps->raw);
    }
    if (gps->dummy) {
        sb_append(sb, ",\n");
        sb_append_indent(sb, indent + 2);
        sb_append(sb, "\"dummy\": true");
    }
    sb_append(sb, "\n");
    sb_append_indent(sb, indent);
    sb_append(sb, "}");
    return true;
}

static char *build_payload_json(AggregatorState *state, double reported_at) {
    StringBuilder sb;
    sb_init(&sb);
    StringBuilder *builder = &sb;
    char ts[64];
    isoformat_utc(reported_at, ts, sizeof(ts));

    sb_append(builder, "{\n  \"reported_at\": ");
    sb_append_json_string(&sb, ts);
    sb_append(builder, ",\n  \"sensors\": {\n");

    for (size_t idx = 0; idx < state->config.sensor_count; ++idx) {
        SensorType sensor = state->config.expected_sensors[idx];
        sb_append_indent(&sb, 4);
        sb_append(builder, "\"");
        sb_append(builder, sensor_name(sensor));
        sb_append(builder, "\": ");
        if (state->has_latest[sensor]) {
            const SensorMessage *msg = &state->latest[sensor];
            switch (sensor) {
                case SENSOR_MPU6050:
                    append_mpu_payload(&sb, msg, 6);
                    break;
                case SENSOR_BMP180:
                    append_bmp_payload(&sb, msg, 6);
                    break;
                case SENSOR_NEO6M:
                    append_gps_payload(&sb, msg, 6);
                    break;
                default:
                    sb_append(builder, "null");
                    break;
            }
        } else {
            sb_append(builder, "null");
        }
        if (idx + 1 < state->config.sensor_count) {
            sb_append(builder, ",\n");
        } else {
            sb_append(builder, "\n");
        }
    }

    sb_append(builder, "  }\n}\n");
    return sb_finalize(&sb);
}

static void log_zero_acc_detection(int count, double magnitude) {
    log_message(LOG_LEVEL_INFO, "MPU6050",
                "Deteccion %d: sin aceleracion lineal (|a|=%.3fg)", count, magnitude);
}

static void aggregator_handle_mpu(AggregatorState *state, const SensorMessage *msg) {
    const MpuData *mpu = &msg->data.mpu;
    double magnitude = sqrt(mpu->ax * mpu->ax + mpu->ay * mpu->ay + mpu->az * mpu->az);
    if (!mpu->dummy && !activity_tracker_zero_sent(state->config.tracker)) {
        double delta = fabs(magnitude - ZERO_ACCEL_REF);
        if (delta <= ZERO_ACCEL_TOLERANCE) {
            double now = msg->timestamp;
            if ((now - state->zero_last_detection) > ZERO_ACCEL_MIN_DELAY) {
                state->zero_acc_count += 1;
                state->zero_last_detection = now;
                log_zero_acc_detection(state->zero_acc_count, magnitude);
                if (state->zero_acc_count >= ZERO_ACCEL_REQUIRED) {
                    activity_tracker_record_zero_signal(state->config.tracker, now, magnitude);
                    log_message(LOG_LEVEL_WARN, "MPU6050", "Senal registrada por aceleracion cero");
                }
            }
        }
    }
}

static void *aggregator_thread_main(void *arg) {
    AggregatorState *state = (AggregatorState *)arg;
    double last_emit = 0.0;
    SensorQueue *queue = state->config.queue;
    while (!(*state->config.stop_flag)) {
        SensorMessage message;
        bool got = sensor_queue_pop(queue, &message, 0.2);
        if (!got) {
            if (*state->config.stop_flag) {
                break;
            }
            continue;
        }

        bool is_dummy = false;
        switch (message.sensor) {
            case SENSOR_MPU6050:
                is_dummy = message.data.mpu.dummy;
                aggregator_handle_mpu(state, &message);
                break;
            case SENSOR_BMP180:
                is_dummy = message.data.bmp.dummy;
                break;
            case SENSOR_NEO6M:
                is_dummy = message.data.gps.dummy;
                break;
            default:
                break;
        }
        activity_tracker_update(state->config.tracker, message.sensor, is_dummy);
        state->latest[message.sensor] = message;
        state->has_latest[message.sensor] = true;

        double now = current_time_seconds();
        if ((now - last_emit) < state->config.emit_interval_seconds) {
            continue;
        }

        char *payload = build_payload_json(state, now);
        if (!payload) {
            continue;
        }
        log_payload(payload);
        if (state->config.send_payload) {
            if (!state->config.send_payload(payload)) {
                log_message(LOG_LEVEL_ERROR, "LORA", "Error inesperado al enviar");
            }
        }
        free(payload);
        last_emit = now;
    }
    return NULL;
}

int aggregator_start(pthread_t *thread, const AggregatorConfig *config) {
    if (!thread || !config || !config->queue || !config->tracker ||
        !config->expected_sensors || config->sensor_count == 0 || !config->stop_flag) {
        return -1;
    }
    AggregatorState *state = (AggregatorState *)calloc(1, sizeof(AggregatorState));
    if (!state) {
        return -1;
    }
    state->config = *config;
    if (pthread_create(thread, NULL, aggregator_thread_main, state) != 0) {
        free(state);
        return -1;
    }
    return 0;
}