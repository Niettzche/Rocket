#include "message_queue.h"

#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct SensorQueue {
    SensorMessage *buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond_nonempty;
    pthread_cond_t cond_nonfull;
};

static void timespec_add_seconds(struct timespec *ts, double seconds) {
    if (!ts) {
        return;
    }
    time_t sec = (time_t)seconds;
    long nsec = (long)((seconds - (double)sec) * 1e9);
    ts->tv_sec += sec;
    ts->tv_nsec += nsec;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

SensorQueue *sensor_queue_create(size_t capacity) {
    if (capacity == 0) {
        capacity = 32;
    }
    SensorQueue *queue = (SensorQueue *)calloc(1, sizeof(SensorQueue));
    if (!queue) {
        return NULL;
    }
    queue->buffer = (SensorMessage *)calloc(capacity, sizeof(SensorMessage));
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }
    queue->capacity = capacity;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond_nonempty, NULL);
    pthread_cond_init(&queue->cond_nonfull, NULL);
    return queue;
}

void sensor_queue_destroy(SensorQueue *queue) {
    if (!queue) {
        return;
    }
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond_nonempty);
    pthread_cond_destroy(&queue->cond_nonfull);
    free(queue->buffer);
    free(queue);
}

void sensor_queue_close(SensorQueue *queue) {
    if (!queue) {
        return;
    }
    pthread_mutex_lock(&queue->mutex);
    queue->closed = true;
    pthread_cond_broadcast(&queue->cond_nonempty);
    pthread_cond_broadcast(&queue->cond_nonfull);
    pthread_mutex_unlock(&queue->mutex);
}

bool sensor_queue_push(SensorQueue *queue, const SensorMessage *message) {
    if (!queue || !message) {
        return false;
    }
    pthread_mutex_lock(&queue->mutex);
    while (!queue->closed && queue->count == queue->capacity) {
        pthread_cond_wait(&queue->cond_nonfull, &queue->mutex);
    }
    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }
    queue->buffer[queue->tail] = *message;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count += 1;
    pthread_cond_signal(&queue->cond_nonempty);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

bool sensor_queue_pop(SensorQueue *queue, SensorMessage *out, double timeout_seconds) {
    if (!queue || !out) {
        return false;
    }
    int timed = (timeout_seconds >= 0.0);
    struct timespec deadline;
    if (timed) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        timespec_add_seconds(&deadline, timeout_seconds);
    }

    pthread_mutex_lock(&queue->mutex);
    while (!queue->closed && queue->count == 0) {
        if (!timed) {
            pthread_cond_wait(&queue->cond_nonempty, &queue->mutex);
        } else {
            int rc = pthread_cond_timedwait(&queue->cond_nonempty, &queue->mutex, &deadline);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return false;
            }
        }
    }

    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    *out = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count -= 1;
    pthread_cond_signal(&queue->cond_nonfull);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}
