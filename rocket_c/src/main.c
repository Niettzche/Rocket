#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "activity_tracker.h"
#include "aggregator.h"
#include "logger.h"
#include "lora_transport.h"
#include "message_queue.h"
#include "sensor_workers.h"
#include "summaries.h"
#include "util.h"

static volatile sig_atomic_t g_stop_flag = 0;

static void handle_signal(int signum) {
    (void)signum;
    g_stop_flag = 1;
}

int main(void) {
    logger_init();
    log_message(LOG_LEVEL_SYS, "SYSTEM", "Arrancando el agregador bonito uwu");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    SensorQueue *queue = sensor_queue_create(128);
    if (!queue) {
        log_message(LOG_LEVEL_ERROR, "SYSTEM", "No pude crear la cola de mensajes");
        return EXIT_FAILURE;
    }

    ActivityTracker tracker;
    activity_tracker_init(&tracker);

    if (!lora_init_tx()) {
        log_message(LOG_LEVEL_ERROR, "LORA", "LoRa tras init: NO LISTO");
    } else {
        log_message(LOG_LEVEL_SYS, "LORA", "LoRa tras init: LISTO");
    }

    AggregatorConfig config = {
        .queue = queue,
        .tracker = &tracker,
        .expected_sensors = SENSOR_LIST,
        .sensor_count = SENSOR_COUNT,
        .send_payload = lora_send_json,
        .emit_interval_seconds = 0.5,
        .stop_flag = &g_stop_flag,
    };

    pthread_t aggregator_thread;
    if (aggregator_start(&aggregator_thread, &config) != 0) {
        log_message(LOG_LEVEL_ERROR, "SYSTEM", "No pude iniciar el agregador");
        sensor_queue_destroy(queue);
        return EXIT_FAILURE;
    }
    log_message(LOG_LEVEL_SYS, "SYSTEM", "Hilo Agregador arriba uwu");

    pthread_t sensor_threads[SENSOR_COUNT];
    if (sensor_threads_start(queue, &g_stop_flag, sensor_threads) != 0) {
        log_message(LOG_LEVEL_ERROR, "SYSTEM", "No pude iniciar los hilos de sensores");
        g_stop_flag = 1;
        sensor_queue_close(queue);
        pthread_join(aggregator_thread, NULL);
        sensor_queue_destroy(queue);
        return EXIT_FAILURE;
    }

    sleep_seconds(0.2);
    log_start_summary(sensor_caps_get());

    while (!g_stop_flag) {
        sleep_seconds(0.2);
    }

    log_message(LOG_LEVEL_SYS, "SYSTEM", "Nos pidieron parar uwu");
    log_message(LOG_LEVEL_SYS, "SYSTEM", "Esperando a que todos terminen uwu");

    sensor_queue_close(queue);

    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        pthread_join(sensor_threads[i], NULL);
    }
    pthread_join(aggregator_thread, NULL);

    log_final_summary(&tracker);

    sensor_queue_destroy(queue);
    log_message(LOG_LEVEL_SYS, "SYSTEM", "Agregador apagado uwu");

    return EXIT_SUCCESS;
}
