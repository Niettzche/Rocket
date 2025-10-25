#include "summaries.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "lora_transport.h"
#include "sensors.h"
#include "util.h"

static void append_name(char *buffer, size_t len, const char *name, bool *first) {
    if (*first) {
        snprintf(buffer, len, "%s", name);
        *first = false;
    } else {
        strncat(buffer, ", ", len - strlen(buffer) - 1);
        strncat(buffer, name, len - strlen(buffer) - 1);
    }
}

void log_start_summary(SensorCaps caps) {
    char activos[128] = "";
    char inactivos[128] = "";
    bool first_act = true;
    bool first_inact = true;

    if (caps.has_mpu) {
        append_name(activos, sizeof(activos), "mpu6050", &first_act);
    } else {
        append_name(inactivos, sizeof(inactivos), "mpu6050", &first_inact);
    }
    if (caps.has_bmp) {
        append_name(activos, sizeof(activos), "bmp180", &first_act);
    } else {
        append_name(inactivos, sizeof(inactivos), "bmp180", &first_inact);
    }
    if (caps.has_gps) {
        append_name(activos, sizeof(activos), "neo6m", &first_act);
    } else {
        append_name(inactivos, sizeof(inactivos), "neo6m", &first_inact);
    }

    const char *activos_txt = first_act ? "ninguno" : activos;
    const char *inactivos_txt = first_inact ? "ninguno" : inactivos;

    log_message(LOG_LEVEL_SYS, "SYSTEM", "===== RESUMEN INICIAL =====");
    log_message(LOG_LEVEL_INFO, "SYSTEM", "Sensores disponibles: %s", activos_txt);
    log_message(LOG_LEVEL_WARN, "SYSTEM", "Sensores NO disponibles: %s", inactivos_txt);
    log_message(LOG_LEVEL_INFO, "SYSTEM", "LoRa: %s", lora_is_ready() ? "LISTO" : "NO LISTO");
}

void log_final_summary(const ActivityTracker *tracker) {
    char reales[128] = "";
    char dummy[128] = "";
    char sin_datos[128] = "";
    bool first_reales = true;
    bool first_dummy = true;
    bool first_sin_datos = true;

    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        SensorType sensor = (SensorType)i;
        const char *name = sensor_name(sensor);
        if (!activity_tracker_seen(tracker, sensor)) {
            append_name(sin_datos, sizeof(sin_datos), name, &first_sin_datos);
        } else if (activity_tracker_last_dummy(tracker, sensor)) {
            append_name(dummy, sizeof(dummy), name, &first_dummy);
        } else {
            append_name(reales, sizeof(reales), name, &first_reales);
        }
    }

    const char *reales_txt = first_reales ? "ninguno" : reales;
    const char *dummy_txt = first_dummy ? "ninguno" : dummy;
    const char *sin_datos_txt = first_sin_datos ? "ninguno" : sin_datos;

    log_message(LOG_LEVEL_SYS, "SYSTEM", "===== RESUMEN FINAL =====");
    log_message(LOG_LEVEL_INFO, "SYSTEM", "Datos REALES recibidos: %s", reales_txt);
    log_message(LOG_LEVEL_WARN, "SYSTEM", "Datos DUMMY (sin hardware): %s", dummy_txt);
    log_message(LOG_LEVEL_ERROR, "SYSTEM", "Sensores sin datos: %s", sin_datos_txt);

    ZeroSignalInfo info = activity_tracker_zero_details(tracker);
    if (info.sent) {
        char ts[64];
        isoformat_utc(info.timestamp, ts, sizeof(ts));
        log_message(LOG_LEVEL_INFO, "SYSTEM", "Senal por aceleracion cero: ENVIADA (t=%s, |a|=%.3fg)",
                    ts, info.magnitude);
    } else {
        log_message(LOG_LEVEL_WARN, "SYSTEM", "Senal por aceleracion cero: NO ENVIADA");
    }
    log_message(LOG_LEVEL_INFO, "SYSTEM", "LoRa: %s", lora_is_ready() ? "LISTO" : "NO LISTO");
}
