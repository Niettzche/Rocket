#include "sensor_workers.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include <linux/i2c-dev.h>

#include "logger.h"
#include "sensor_message.h"
#include "util.h"

#define I2C_DEVICE "/dev/i2c-1"
#define MPU_ADDR 0x68
#define ACCEL_SCALE 16384.0
#define GYRO_SCALE 131.0
#define N_CALIB 500
#define ALPHA 0.3
#define ALPHA_FUSION 0.96
#define SMOOTHING 0.9
#define DT_SLEEP 0.05

#define BMP_PORT "/dev/ttyUSB0"
#define BMP_BAUD B9600
#define BMP_TIMEOUT 0.2

#define GPS_PORT "/dev/serial0"
#define GPS_BAUD B9600
#define GPS_TIMEOUT 0.4

typedef struct {
    double pitch;
    double roll;
    double yaw;
    double pitch_smooth;
    double roll_smooth;
} MpuState;

typedef struct {
    SensorQueue *queue;
    volatile sig_atomic_t *stop_flag;
} WorkerContext;

static SensorCaps g_caps = {false, false, false};

static double to_degrees(double radians) {
    return radians * (180.0 / M_PI);
}

static int open_i2c_device(void) {
    int fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0) {
        log_message(LOG_LEVEL_ERROR, "MPU6050", "no pude abrir %s: %s", I2C_DEVICE, strerror(errno));
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, MPU_ADDR) < 0) {
        log_message(LOG_LEVEL_ERROR, "MPU6050", "ioctl fallo: %s", strerror(errno));
        close(fd);
        return -1;
    }
    uint8_t buffer[2] = {0x6B, 0x00};
    if (write(fd, buffer, 2) != 2) {
        log_message(LOG_LEVEL_ERROR, "MPU6050", "no pude despertar el sensor: %s", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static bool i2c_read_bytes(int fd, uint8_t reg, uint8_t *data, size_t length) {
    if (write(fd, &reg, 1) != 1) {
        return false;
    }
    ssize_t read_bytes = read(fd, data, length);
    return read_bytes == (ssize_t)length;
}

static bool mpu_read_raw(int fd, int16_t accel[3], int16_t gyro[3]) {
    uint8_t buffer[14];
    if (!i2c_read_bytes(fd, 0x3B, buffer, sizeof(buffer))) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        accel[i] = (int16_t)((buffer[i * 2] << 8) | buffer[i * 2 + 1]);
        gyro[i] = (int16_t)((buffer[8 + i * 2] << 8) | buffer[8 + i * 2 + 1]);
    }
    return true;
}

static bool mpu_calibrate(int fd, double accel_offsets[3], double gyro_offsets[3]) {
    memset(accel_offsets, 0, sizeof(double) * 3);
    memset(gyro_offsets, 0, sizeof(double) * 3);
    int16_t accel[3];
    int16_t gyro[3];
    for (int i = 0; i < N_CALIB; ++i) {
        if (!mpu_read_raw(fd, accel, gyro)) {
            return false;
        }
        for (int axis = 0; axis < 3; ++axis) {
            accel_offsets[axis] += accel[axis];
            gyro_offsets[axis] += gyro[axis];
        }
        sleep_seconds(0.002);
    }
    for (int axis = 0; axis < 3; ++axis) {
        accel_offsets[axis] /= N_CALIB;
        gyro_offsets[axis] /= N_CALIB;
    }
    log_message(LOG_LEVEL_INFO, "MPU6050", "Calibracion completada.");
    return true;
}

static void complementary_filter(double ax, double ay, double az, double gx, double gy, double gz, double dt, MpuState *state) {
    double pitch_acc = to_degrees(atan2(-ax, sqrt(ay * ay + az * az)));
    double roll_acc = (fabs(az) >= 0.01) ? to_degrees(atan2(ay, az)) : state->roll;

    state->pitch = ALPHA_FUSION * (state->pitch + gx * dt) + (1.0 - ALPHA_FUSION) * pitch_acc;
    state->roll = ALPHA_FUSION * (state->roll + gy * dt) + (1.0 - ALPHA_FUSION) * roll_acc;
    state->yaw += gz * dt;

    state->pitch_smooth = SMOOTHING * state->pitch_smooth + (1.0 - SMOOTHING) * state->pitch;
    state->roll_smooth = SMOOTHING * state->roll_smooth + (1.0 - SMOOTHING) * state->roll;
}

static void *mpu_worker(void *arg) {
    WorkerContext *ctx = (WorkerContext *)arg;
    SensorQueue *queue = ctx->queue;
    volatile sig_atomic_t *stop_flag = ctx->stop_flag;
    free(ctx);

    int fd = open_i2c_device();
    if (fd < 0) {
        log_message(LOG_LEVEL_WARN, "MPU6050", "sin sensor, usando datos dummy");
        double phase = 0.0;
        while (!(*stop_flag)) {
            double now = current_time_seconds();
            SensorMessage msg;
            memset(&msg, 0, sizeof(msg));
            msg.sensor = SENSOR_MPU6050;
            msg.timestamp = now;
            msg.data.mpu.ax = 0.01 * sin(phase);
            msg.data.mpu.ay = 0.01 * cos(phase);
            msg.data.mpu.az = 1.0;
            msg.data.mpu.gx = 0.1 * sin(phase);
            msg.data.mpu.gy = 0.1 * cos(phase);
            msg.data.mpu.gz = 0.0;
            msg.data.mpu.pitch = 0.0;
            msg.data.mpu.roll = 0.0;
            msg.data.mpu.yaw = 0.0;
            msg.data.mpu.dummy = true;
            sensor_queue_push(queue, &msg);
            phase += 0.05;
            sleep_seconds(0.05);
        }
        return NULL;
    }

    g_caps.has_mpu = true;
    log_message(LOG_LEVEL_INFO, "MPU6050", "Calibrando el sensor");
    double accel_offsets[3];
    double gyro_offsets[3];
    if (!mpu_calibrate(fd, accel_offsets, gyro_offsets)) {
        log_message(LOG_LEVEL_ERROR, "MPU6050", "fallo la calibracion");
        close(fd);
        g_caps.has_mpu = false;
        return NULL;
    }

    MpuState state = {0};
    double alpha_ax = 0.0, alpha_ay = 0.0, alpha_az = 0.0;
    double alpha_gx = 0.0, alpha_gy = 0.0, alpha_gz = 0.0;
    double last_time = current_time_seconds();
    log_message(LOG_LEVEL_DEBUG, "MPU6050", "Arranca el bucle de captura");

    while (!(*stop_flag)) {
        int16_t accel_raw[3];
        int16_t gyro_raw[3];
        if (!mpu_read_raw(fd, accel_raw, gyro_raw)) {
            log_message(LOG_LEVEL_ERROR, "MPU6050", "no pude leer del sensor");
            break;
        }
        double ax = ((double)accel_raw[0] - accel_offsets[0]) / ACCEL_SCALE;
        double ay = ((double)accel_raw[1] - accel_offsets[1]) / ACCEL_SCALE;
        double az = ((double)accel_raw[2] - accel_offsets[2]) / ACCEL_SCALE;
        double gx = ((double)gyro_raw[0] - gyro_offsets[0]) / GYRO_SCALE;
        double gy = ((double)gyro_raw[1] - gyro_offsets[1]) / GYRO_SCALE;
        double gz = ((double)gyro_raw[2] - gyro_offsets[2]) / GYRO_SCALE;

        alpha_ax = ALPHA * ax + (1.0 - ALPHA) * alpha_ax;
        alpha_ay = ALPHA * ay + (1.0 - ALPHA) * alpha_ay;
        alpha_az = ALPHA * az + (1.0 - ALPHA) * alpha_az;
        alpha_gx = ALPHA * gx + (1.0 - ALPHA) * alpha_gx;
        alpha_gy = ALPHA * gy + (1.0 - ALPHA) * alpha_gy;
        alpha_gz = ALPHA * gz + (1.0 - ALPHA) * alpha_gz;

        double now = current_time_seconds();
        double dt = now - last_time;
        if (dt <= 0.0) {
            dt = 0.001;
        }
        last_time = now;

        complementary_filter(alpha_ax, alpha_ay, alpha_az, alpha_gx, alpha_gy, alpha_gz, dt, &state);

        SensorMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.sensor = SENSOR_MPU6050;
        msg.timestamp = now;
        msg.data.mpu.ax = alpha_ax;
        msg.data.mpu.ay = alpha_ay;
        msg.data.mpu.az = alpha_az;
        msg.data.mpu.gx = alpha_gx;
        msg.data.mpu.gy = alpha_gy;
        msg.data.mpu.gz = alpha_gz;
        msg.data.mpu.pitch = state.pitch_smooth;
        msg.data.mpu.roll = state.roll_smooth;
        msg.data.mpu.yaw = state.yaw;
        msg.data.mpu.dummy = false;

        if (!sensor_queue_push(queue, &msg)) {
            break;
        }

        sleep_seconds(DT_SLEEP);
    }

    log_message(LOG_LEVEL_DEBUG, "MPU6050", "Bucle de captura detenido");
    close(fd);
    return NULL;
}

static int configure_serial(int fd, speed_t baud, double timeout) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        return -1;
    }
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VTIME] = (cc_t)(timeout * 10);
    tty.c_cc[VMIN] = 0;

    return tcsetattr(fd, TCSANOW, &tty);
}

static ssize_t serial_readline(int fd, char *buffer, size_t len, double timeout) {
    if (len == 0) {
        return -1;
    }
    size_t pos = 0;
    while (pos + 1 < len) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval tv;
        tv.tv_sec = (time_t)timeout;
        tv.tv_usec = (suseconds_t)((timeout - (double)tv.tv_sec) * 1e6);
        int rv = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (rv == 0) {
            return 0;  // timeout
        }
        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        char ch;
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            return -1;
        }
        if (ch == '\r') {
            continue;
        }
        buffer[pos++] = ch;
        if (ch == '\n') {
            break;
        }
    }
    buffer[pos] = '\0';
    return (ssize_t)pos;
}

static void *bmp_worker(void *arg) {
    WorkerContext *ctx = (WorkerContext *)arg;
    SensorQueue *queue = ctx->queue;
    volatile sig_atomic_t *stop_flag = ctx->stop_flag;
    free(ctx);

    int fd = open(BMP_PORT, O_RDONLY | O_NOCTTY | O_SYNC);
    if (fd < 0 || configure_serial(fd, BMP_BAUD, BMP_TIMEOUT) != 0) {
        if (fd >= 0) {
            close(fd);
        }
        log_message(LOG_LEVEL_WARN, "BMP180", "sin sensor, usando datos dummy");
        double temp = 25.0;
        double pres = 1013.25;
        while (!(*stop_flag)) {
            temp += 0.01;
            pres += 0.02;
            SensorMessage msg;
            memset(&msg, 0, sizeof(msg));
            msg.sensor = SENSOR_BMP180;
            msg.timestamp = current_time_seconds();
            msg.data.bmp.has_temperature = true;
            msg.data.bmp.has_pressure = true;
            msg.data.bmp.temperature = temp;
            msg.data.bmp.pressure = pres;
            msg.data.bmp.dummy = true;
            snprintf(msg.data.bmp.raw, sizeof(msg.data.bmp.raw), "T=%.2f,P=%.2f", temp, pres);
            msg.data.bmp.has_raw = true;
            sensor_queue_push(queue, &msg);
            sleep_seconds(0.2);
        }
        return NULL;
    }

    g_caps.has_bmp = true;
    log_message(LOG_LEVEL_DEBUG, "BMP180", "Escuchando lecturas del Arduino");
    while (!(*stop_flag)) {
        char line[128];
        ssize_t n = serial_readline(fd, line, sizeof(line), BMP_TIMEOUT);
        if (n < 0) {
            log_message(LOG_LEVEL_ERROR, "BMP180", "error leyendo del puerto");
            break;
        }
        if (n == 0) {
            continue;
        }
        // remove newline
        if (n > 0 && line[n - 1] == '\n') {
            line[n - 1] = '\0';
        }
        log_message(LOG_LEVEL_DEBUG, "BMP180", "dato crudo: %s", line);
        SensorMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.sensor = SENSOR_BMP180;
        msg.timestamp = current_time_seconds();
        msg.data.bmp.has_raw = true;
        strncpy(msg.data.bmp.raw, line, sizeof(msg.data.bmp.raw) - 1);
        msg.data.bmp.raw[sizeof(msg.data.bmp.raw) - 1] = '\0';
        msg.data.bmp.dummy = false;
        if (!sensor_queue_push(queue, &msg)) {
            break;
        }
    }

    close(fd);
    return NULL;
}

static double parse_coordinate(const char *value) {
    if (!value || !*value) {
        return 0.0;
    }
    double raw = atof(value);
    double degrees = floor(raw / 100.0);
    double minutes = raw - degrees * 100.0;
    return degrees + minutes / 60.0;
}

static void format_fix_time(const char *nmea_time, char *buffer, size_t len) {
    if (!nmea_time || strlen(nmea_time) < 6 || len < 9) {
        buffer[0] = '\0';
        return;
    }
    snprintf(buffer, len, "%c%c:%c%c:%c%c",
             nmea_time[0], nmea_time[1],
             nmea_time[2], nmea_time[3],
             nmea_time[4], nmea_time[5]);
}

static bool parse_gga_sentence(const char *line, GpsData *out) {
    char copy[256];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    char *tokens[16] = {0};
    size_t count = 0;
    char *token = strtok(copy, ",");
    while (token && count < 16) {
        tokens[count++] = token;
        token = strtok(NULL, ",");
    }
    if (count < 10) {
        return false;
    }
    const char *time_field = tokens[1];
    const char *lat_field = tokens[2];
    const char *lat_hemi = tokens[3];
    const char *lon_field = tokens[4];
    const char *lon_hemi = tokens[5];
    const char *fix_quality = tokens[6];
    const char *alt_field = tokens[9];

    if (!fix_quality || fix_quality[0] == '0') {
        return false;
    }

    if (lat_field && lat_field[0]) {
        double lat = parse_coordinate(lat_field);
        if (lat_hemi && lat_hemi[0] == 'S') {
            lat = -lat;
        }
        out->latitude = lat;
        out->has_latitude = true;
    }
    if (lon_field && lon_field[0]) {
        double lon = parse_coordinate(lon_field);
        if (lon_hemi && lon_hemi[0] == 'W') {
            lon = -lon;
        }
        out->longitude = lon;
        out->has_longitude = true;
    }
    if (alt_field && alt_field[0]) {
        out->altitude = atof(alt_field);
        out->has_altitude = true;
    }
    if (time_field && time_field[0]) {
        format_fix_time(time_field, out->fix_time, sizeof(out->fix_time));
        out->has_fix_time = true;
    }
    out->dummy = false;
    out->has_raw = true;
    strncpy(out->raw, line, sizeof(out->raw) - 1);
    out->raw[sizeof(out->raw) - 1] = '\0';
    return true;
}

static void *gps_worker(void *arg) {
    WorkerContext *ctx = (WorkerContext *)arg;
    SensorQueue *queue = ctx->queue;
    volatile sig_atomic_t *stop_flag = ctx->stop_flag;
    free(ctx);

    int fd = open(GPS_PORT, O_RDONLY | O_NOCTTY | O_SYNC);
    if (fd < 0 || configure_serial(fd, GPS_BAUD, GPS_TIMEOUT) != 0) {
        if (fd >= 0) {
            close(fd);
        }
        log_message(LOG_LEVEL_WARN, "NEO6M", "sin GPS, usando datos dummy");
        double lat = 25.651;
        double lon = -100.289;
        double alt = 512.0;
        while (!(*stop_flag)) {
            lat += 1e-5;
            lon -= 1e-5;
            SensorMessage msg;
            memset(&msg, 0, sizeof(msg));
            msg.sensor = SENSOR_NEO6M;
            msg.timestamp = current_time_seconds();
            msg.data.gps.latitude = lat;
            msg.data.gps.longitude = lon;
            msg.data.gps.altitude = alt;
            msg.data.gps.has_latitude = true;
            msg.data.gps.has_longitude = true;
            msg.data.gps.has_altitude = true;
            msg.data.gps.dummy = true;
            msg.data.gps.has_fix_time = true;
            strncpy(msg.data.gps.fix_time, "DUMMY", sizeof(msg.data.gps.fix_time) - 1);
            msg.data.gps.fix_time[sizeof(msg.data.gps.fix_time) - 1] = '\0';
            msg.data.gps.has_raw = true;
            strncpy(msg.data.gps.raw, "$GPGGA,DUMMY", sizeof(msg.data.gps.raw) - 1);
            msg.data.gps.raw[sizeof(msg.data.gps.raw) - 1] = '\0';
            sensor_queue_push(queue, &msg);
            sleep_seconds(0.5);
        }
        return NULL;
    }

    g_caps.has_gps = true;
    log_message(LOG_LEVEL_INFO, "NEO6M", "Esperando sentencias NMEA");
    while (!(*stop_flag)) {
        char line[256];
        ssize_t n = serial_readline(fd, line, sizeof(line), GPS_TIMEOUT);
        if (n < 0) {
            log_message(LOG_LEVEL_ERROR, "NEO6M", "error leyendo del GPS");
            break;
        }
        if (n == 0) {
            continue;
        }
        if (n > 0 && line[n - 1] == '\n') {
            line[n - 1] = '\0';
        }
        if (strncmp(line, "$GPGGA", 6) != 0 && strncmp(line, "$GPRMC", 6) != 0) {
            continue;
        }
        SensorMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.sensor = SENSOR_NEO6M;
        msg.timestamp = current_time_seconds();
        if (!parse_gga_sentence(line, &msg.data.gps)) {
            continue;
        }
        msg.data.gps.has_raw = true;
        strncpy(msg.data.gps.raw, line, sizeof(msg.data.gps.raw) - 1);
        msg.data.gps.raw[sizeof(msg.data.gps.raw) - 1] = '\0';
        log_message(LOG_LEVEL_DEBUG, "NEO6M", "lat=%.6f lon=%.6f alt=%.1f hora=%s",
                    msg.data.gps.latitude,
                    msg.data.gps.longitude,
                    msg.data.gps.altitude,
                    msg.data.gps.has_fix_time ? msg.data.gps.fix_time : "-");
        if (!sensor_queue_push(queue, &msg)) {
            break;
        }
    }
    close(fd);
    return NULL;
}

SensorCaps sensor_caps_get(void) {
    return g_caps;
}

int sensor_threads_start(SensorQueue *queue, volatile sig_atomic_t *stop_flag, pthread_t threads[SENSOR_COUNT]) {
    if (!queue || !stop_flag || !threads) {
        return -1;
    }

    void *(*workers[SENSOR_COUNT])(void *) = {mpu_worker, bmp_worker, gps_worker};
    const char *names[SENSOR_COUNT] = {"MPU6050", "BMP180", "NEO6M"};

    for (size_t i = 0; i < SENSOR_COUNT; ++i) {
        WorkerContext *ctx = (WorkerContext *)malloc(sizeof(WorkerContext));
        if (!ctx) {
            log_message(LOG_LEVEL_ERROR, "SYSTEM", "sin memoria para contexto del hilo %s", names[i]);
            for (size_t j = 0; j < i; ++j) {
                pthread_cancel(threads[j]);
            }
            return -1;
        }
        ctx->queue = queue;
        ctx->stop_flag = stop_flag;
        if (pthread_create(&threads[i], NULL, workers[i], ctx) != 0) {
            log_message(LOG_LEVEL_ERROR, "SYSTEM", "no pude crear el hilo %s", names[i]);
            free(ctx);
            for (size_t j = 0; j < i; ++j) {
                pthread_cancel(threads[j]);
            }
            return -1;
        }
        log_message(LOG_LEVEL_SYS, "SYSTEM", "Hilo %s arriba uwu", names[i]);
#if defined(__linux__)
        pthread_setname_np(threads[i], names[i]);
#endif
    }

    return 0;
}

