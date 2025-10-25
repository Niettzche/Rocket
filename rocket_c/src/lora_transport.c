#include "lora_transport.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

#define LORA_FREQ_HZ 433000000UL
#define LORA_SF 7
#define LORA_MAX_BYTES 200

#if defined(__has_include)
#  if __has_include(<loralib.h>)
#    define HAVE_LORALIB 1
#    include <loralib.h>
#  endif
#endif

#ifndef HAVE_LORALIB
static int __attribute__((unused)) loralib_init(int spi_channel, unsigned long freq_hz, int sf) {
    (void)spi_channel;
    (void)freq_hz;
    (void)sf;
    return -1;
}

static int __attribute__((unused)) loralib_send(const uint8_t *buffer, size_t length) {
    (void)buffer;
    (void)length;
    return -1;
}
#endif

static bool lora_ready = false;

bool lora_init_tx(void) {
    int rc = loralib_init(0, LORA_FREQ_HZ, LORA_SF);
    if (rc == 0) {
        lora_ready = true;
        log_message(LOG_LEVEL_SYS, "LORA", "cargando: TX listo @ %lu Hz, SF%d", LORA_FREQ_HZ, LORA_SF);
    } else {
        lora_ready = false;
        log_message(LOG_LEVEL_ERROR, "LORA", "no se pudo inicializar (modo simulacion)");
    }
    return lora_ready;
}

bool lora_is_ready(void) {
    return lora_ready;
}

static bool send_frame(const uint8_t *frame, size_t length, size_t index, size_t total) {
#ifdef HAVE_LORALIB
    int rc = loralib_send(frame, length);
    if (rc != 0) {
        log_message(LOG_LEVEL_ERROR, "LORA", "fallo envio (frame %zu/%zu)", index, total);
        return false;
    }
    log_message(LOG_LEVEL_INFO, "LORA", "Enviado frame %zu/%zu (%zu B)", index, total, length);
    return true;
#else
    (void)frame;
    (void)length;
    (void)index;
    (void)total;
    log_message(LOG_LEVEL_WARN, "LORA", "loralib no disponible; omito envio (frame %zu/%zu)", index, total);
    return true;
#endif
}

bool lora_send_json(const char *json_payload) {
    if (!json_payload) {
        return false;
    }
    size_t payload_len = strlen(json_payload);
    const char *topic = "sensors";
    size_t topic_len = strlen(topic);
    if (topic_len > 15) {
        topic_len = 15;
    }

    if (!lora_ready) {
        log_message(LOG_LEVEL_WARN, "LORA", "no listo; omito envio (modo prueba)");
        return true;
    }

    size_t head_fixed = 1 + 1 + topic_len + 1 + 1;
    size_t room = (LORA_MAX_BYTES > head_fixed) ? (LORA_MAX_BYTES - head_fixed) : 1;
    if (room == 0) {
        room = 1;
    }

    size_t total_frames = (payload_len + room - 1) / room;
    if (total_frames == 0) {
        total_frames = 1;
    }

    for (size_t idx = 0; idx < total_frames; ++idx) {
        size_t start = idx * room;
        size_t remaining = payload_len > start ? (payload_len - start) : 0;
        size_t chunk_len = remaining > room ? room : remaining;

        size_t frame_len = head_fixed + chunk_len;
        uint8_t *frame = (uint8_t *)malloc(frame_len);
        if (!frame) {
            log_message(LOG_LEVEL_ERROR, "LORA", "sin memoria para frame LoRa");
            return false;
        }
        size_t pos = 0;
        frame[pos++] = 'J';
        frame[pos++] = (uint8_t)topic_len;
        memcpy(frame + pos, topic, topic_len);
        pos += topic_len;
        frame[pos++] = (uint8_t)((idx + 1) & 0xFF);
        frame[pos++] = (uint8_t)(total_frames & 0xFF);
        if (chunk_len > 0) {
            memcpy(frame + pos, json_payload + start, chunk_len);
        }

        bool ok = send_frame(frame, head_fixed + chunk_len, idx + 1, total_frames);
        free(frame);
        if (!ok) {
            return false;
        }
    }
    return true;
}
