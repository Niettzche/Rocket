#ESTE TIENE MAS PRIORIDAD QUE LOS OTROS
import smbus
import time
import csv
from datetime import datetime
from math import atan2, sqrt, degrees
from typing import Optional

import RPi.GPIO as GPIO

#--Constantes
I2C_BUS = 1
MPU_ADDR = 0x68
ACCEL_SCALE = 16384.0
GYRO_SCALE = 131.0
N_CALIB = 500
ALPHA = 0.3             
ALPHA_FUSION = 0.96     
SMOOTHING = 0.9         
DT_SLEEP = 0.05         

#--Config de la aceleracion
TOLERANCE = 0.04   
QUIET_DELAY = 1.0
GPIO_PIN = 26 
GPIO_SECOND_PIN = 7
GPIO_ACTIVATED_MSG = f"\033[31m[GPIO {GPIO_PIN} ACTIVADO]\033[0m"
GPIO_SECOND_ACTIVATED_MSG = f"\033[31m[GPIO {GPIO_SECOND_PIN} ACTIVADO]\033[0m"

_bus: Optional[smbus.SMBus] = None


def _reset_bus() -> None:
    """Close and discard the cached SMBus instance."""
    global _bus
    if _bus is not None:
        try:
            _bus.close()
        except Exception:
            pass
    _bus = None


def _get_bus() -> smbus.SMBus:
    """Return a ready-to-use SMBus instance, creating it on demand."""
    global _bus
    if _bus is None:
        bus = smbus.SMBus(I2C_BUS)
        try:
            bus.write_byte_data(MPU_ADDR, 0x6B, 0)
        except Exception:
            bus.close()
            raise
        _bus = bus
    return _bus

def read_word(reg: int, retries: int = 3, retry_delay: float = 0.05) -> int:
    """Read a 16-bit value from the sensor with simple retry logic."""
    attempt = 0
    while True:
        attempt += 1
        try:
            bus = _get_bus()
            h = bus.read_byte_data(MPU_ADDR, reg)
            l = bus.read_byte_data(MPU_ADDR, reg + 1)
            val = (h << 8) + l
            return -((65535 - val) + 1) if val >= 0x8000 else val
        except OSError:
            _reset_bus()
            if attempt >= retries:
                raise
            time.sleep(retry_delay)

def read_accel_gyro(offsets):
    ax = (read_word(0x3B) - offsets['accel']['x']) / ACCEL_SCALE
    ay = (read_word(0x3D) - offsets['accel']['y']) / ACCEL_SCALE
    az = (read_word(0x3F) - offsets['accel']['z']) / ACCEL_SCALE
    gx = (read_word(0x43) - offsets['gyro']['x']) / GYRO_SCALE
    gy = (read_word(0x45) - offsets['gyro']['y']) / GYRO_SCALE
    gz = (read_word(0x47) - offsets['gyro']['z']) / GYRO_SCALE
    return ax, ay, az, gx, gy, gz

def calibrate_sensor():
    _get_bus()
    print("Calibrando... No muevas el sensor.")
    accel_offset = {'x': 0, 'y': 0, 'z': 0}
    gyro_offset = {'x': 0, 'y': 0, 'z': 0}
    for _ in range(N_CALIB):
        accel_offset['x'] += read_word(0x3B)
        accel_offset['y'] += read_word(0x3D)
        accel_offset['z'] += read_word(0x3F)
        gyro_offset['x'] += read_word(0x43)
        gyro_offset['y'] += read_word(0x45)
        gyro_offset['z'] += read_word(0x47)
        time.sleep(0.002)

    accel_offset = {k: v / N_CALIB for k, v in accel_offset.items()}
    gyro_offset = {k: v / N_CALIB for k, v in gyro_offset.items()}
    print("Calibracion completada.\n")
    return {'accel': accel_offset, 'gyro': gyro_offset}

def complementary_filter(ax, ay, az, gx, gy, gz, dt, state):
    # Acelerometro 
    pitch_acc = degrees(atan2(-ax, sqrt(ay**2 + az**2)))
    roll_acc = degrees(atan2(ay, az)) if abs(az) >= 0.01 else state['roll']

    state['pitch'] = ALPHA_FUSION * (state['pitch'] + gx * dt) + (1 - ALPHA_FUSION) * pitch_acc
    state['roll'] = ALPHA_FUSION * (state['roll'] + gy * dt) + (1 - ALPHA_FUSION) * roll_acc
    state['yaw'] += gz * dt

    state['pitch_smooth'] = SMOOTHING * state['pitch_smooth'] + (1 - SMOOTHING) * state['pitch']
    state['roll_smooth'] = SMOOTHING * state['roll_smooth'] + (1 - SMOOTHING) * state['roll']
    return state

def main():
    offsets = calibrate_sensor()
    state = {'pitch': 0.0, 'roll': 0.0, 'yaw': 0.0,
             'pitch_smooth': 0.0, 'roll_smooth': 0.0}
    alpha_filter = {'ax': 0.0, 'ay': 0.0, 'az': 0.0,
                    'gx': 0.0, 'gy': 0.0, 'gz': 0.0}

    quiet_count = 0
    last_detection = 0  # tiempo de la ultima deteccion
    signal_sent = False
    gpio_initialized = False

    try:
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        GPIO.setup(GPIO_PIN, GPIO.OUT, initial=GPIO.LOW)
        GPIO.setup(GPIO_SECOND_PIN, GPIO.OUT, initial=GPIO.LOW)
        gpio_initialized = True

        filename = f"mpu6050_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        with open(filename, mode='w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([
                "Tiempo", "Acel_X", "Acel_Y", "Acel_Z",
                "Gyro_X", "Gyro_Y", "Gyro_Z",
                "Pitch", "Roll", "Yaw"
            ])

            last_time = time.time()
            ciclo = 0

            try:
                while True:
                    current_time = time.time()
                    dt = current_time - last_time
                    last_time = current_time

                    ax, ay, az, gx, gy, gz = read_accel_gyro(offsets)

                    alpha_filter['ax'] = ALPHA * ax + (1 - ALPHA) * alpha_filter['ax']
                    alpha_filter['ay'] = ALPHA * ay + (1 - ALPHA) * alpha_filter['ay']
                    alpha_filter['az'] = ALPHA * az + (1 - ALPHA) * alpha_filter['az']
                    alpha_filter['gx'] = ALPHA * gx + (1 - ALPHA) * alpha_filter['gx']
                    alpha_filter['gy'] = ALPHA * gy + (1 - ALPHA) * alpha_filter['gy']
                    alpha_filter['gz'] = ALPHA * gz + (1 - ALPHA) * alpha_filter['gz']

                    state = complementary_filter(
                        alpha_filter['ax'], alpha_filter['ay'], alpha_filter['az'],
                        alpha_filter['gx'], alpha_filter['gy'], alpha_filter['gz'],
                        dt, state
                    )

                    # ---- Detección de aceleración 0
                    a_magnitude = sqrt(
                        alpha_filter['ax']**2 +
                        alpha_filter['ay']**2 +
                        alpha_filter['az']**2
                    )

                    if abs(a_magnitude - 1.0) < TOLERANCE:
                        if (current_time - last_detection) > QUIET_DELAY:
                            quiet_count += 1
                            last_detection = current_time
                            print(f"Detección {quiet_count}: sin aceleración lineal")

                            if quiet_count == 2 and not signal_sent:
                                print("Segunda detección de aceleracion cero")
                                GPIO.output(GPIO_PIN, GPIO.HIGH)
                                GPIO.output(GPIO_SECOND_PIN, GPIO.HIGH)
                                print(GPIO_ACTIVATED_MSG)
                                print(GPIO_SECOND_ACTIVATED_MSG)
                                print("[INFO] HACIENDO TIME SLEEP UWU")
                                time.sleep(60)
                                signal_sent = True
                                break

                    if ciclo % 10 == 0:
                        print(f"[{datetime.now().strftime('%H:%M:%S')}]")
                        print(f"Acel: X={alpha_filter['ax']:.2f}g  Y={alpha_filter['ay']:.2f}g  Z={alpha_filter['az']:.2f}g")
                        print(f"Ángulos: Pitch={state['pitch_smooth']:.2f}°  Roll={state['roll_smooth']:.2f}°  Yaw={state['yaw']:.2f}°\n")

                    writer.writerow([
                        datetime.now().isoformat(),
                        round(alpha_filter['ax'], 3), round(alpha_filter['ay'], 3), round(alpha_filter['az'], 3),
                        round(alpha_filter['gx'], 3), round(alpha_filter['gy'], 3), round(alpha_filter['gz'], 3),
                        round(state['pitch_smooth'], 2), round(state['roll_smooth'], 2), round(state['yaw'], 2)
                    ])

                    ciclo += 1
                    time.sleep(DT_SLEEP)

            except KeyboardInterrupt:
                print("Lectura detenida por el usuario.")
    finally:
        if gpio_initialized:
            if signal_sent:
                print("GPIO mantenido en HIGH; recuerde reiniciar manualmente si es necesario.")
            else:
                GPIO.output(GPIO_PIN, GPIO.LOW)
                GPIO.output(GPIO_SECOND_PIN, GPIO.LOW)
                GPIO.cleanup()

if __name__ == "__main__":
    main()
