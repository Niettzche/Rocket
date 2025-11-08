#include <Arduino.h>
#include <stdio.h>

constexpr uint32_t SIM_PERIOD_MS = 2000;

struct SimTelemetryState {
  float accelX = -0.27f;
  float accelY = -0.24f;
  float accelZ = 0.92f;
  float gyroX = 63.0f;
  float gyroY = 123.0f;
  float gyroZ = -53.0f;
  float pitch = 9.7f;
  float roll = 8.1f;
  float yaw = 74.9f;
  float temperature = 29.0f;
  float pressure = 985.8f;
  float latitude = -78.83376f;
  float longitude = 47.98929f;
  float altitude = 281.5f;
};

SimTelemetryState simState;
uint32_t lastSimMillis = 0;

String isoTimestamp(uint16_t offsetMs = 0) {
  const uint32_t now = millis() + offsetMs;
  const uint16_t milli = now % 1000;
  const uint32_t totalSeconds = now / 1000;
  const uint8_t seconds = totalSeconds % 60;
  const uint8_t minutes = (totalSeconds / 60) % 60;
  const uint8_t hours = (totalSeconds / 3600) % 24;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "2025-01-01T%02u:%02u:%02u.%03uZ", hours, minutes, seconds, milli);
  return String(buffer);
}

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

float randomStep(float magnitude) {
  const int16_t bucketValue = random(-100, 101);
  return (bucketValue / 100.0f) * magnitude;
}

String buildTelemetryPayload() {
  simState.accelX = clampFloat(simState.accelX + randomStep(0.08f), -2.0f, 2.0f);
  simState.accelY = clampFloat(simState.accelY + randomStep(0.08f), -2.0f, 2.0f);
  simState.accelZ = clampFloat(simState.accelZ + randomStep(0.05f), 0.7f, 1.2f);
  simState.gyroX = clampFloat(simState.gyroX + randomStep(4.0f), -250.0f, 250.0f);
  simState.gyroY = clampFloat(simState.gyroY + randomStep(4.0f), -250.0f, 250.0f);
  simState.gyroZ = clampFloat(simState.gyroZ + randomStep(4.0f), -250.0f, 250.0f);
  simState.pitch = clampFloat(simState.pitch + randomStep(1.8f), -45.0f, 45.0f);
  simState.roll = clampFloat(simState.roll + randomStep(1.8f), -45.0f, 45.0f);
  simState.yaw = clampFloat(simState.yaw + randomStep(3.5f), 0.0f, 360.0f);
  simState.temperature = clampFloat(simState.temperature + randomStep(0.4f), 15.0f, 40.0f);
  simState.pressure = clampFloat(simState.pressure + randomStep(0.6f), 980.0f, 1040.0f);
  simState.latitude = clampFloat(simState.latitude + randomStep(0.0025f), -90.0f, 90.0f);
  simState.longitude = clampFloat(simState.longitude + randomStep(0.0035f), -180.0f, 180.0f);
  simState.altitude = clampFloat(simState.altitude + randomStep(8.5f), 0.0f, 2000.0f);

  const String reportedAt = isoTimestamp();
  const String mpuTimestamp = isoTimestamp(5);
  const String bmpTimestamp = isoTimestamp(60);
  const String gpsTimestamp = isoTimestamp(110);
  const String gpsFixTime = isoTimestamp(200);

  String payload;
  payload.reserve(512);
  payload += '{';
  payload += "\"reported_at\":\"";
  payload += reportedAt;
  payload += "\",";
  payload += "\"sensors\":{";
  payload += "\"mpu6050\":{";
  payload += "\"timestamp\":\"";
  payload += mpuTimestamp;
  payload += "\",";
  payload += "\"accel_g\":{";
  payload += "\"ax\":";
  payload += String(simState.accelX, 3);
  payload += ",\"ay\":";
  payload += String(simState.accelY, 3);
  payload += ",\"az\":";
  payload += String(simState.accelZ, 3);
  payload += "},";
  payload += "\"gyro_dps\":{";
  payload += "\"gx\":";
  payload += String(simState.gyroX, 2);
  payload += ",\"gy\":";
  payload += String(simState.gyroY, 2);
  payload += ",\"gz\":";
  payload += String(simState.gyroZ, 2);
  payload += "},";
  payload += "\"attitude_deg\":{";
  payload += "\"pitch\":";
  payload += String(simState.pitch, 2);
  payload += ",\"roll\":";
  payload += String(simState.roll, 2);
  payload += ",\"yaw\":";
  payload += String(simState.yaw, 2);
  payload += "}";
  payload += "},";
  payload += "\"bmp180\":{";
  payload += "\"timestamp\":\"";
  payload += bmpTimestamp;
  payload += "\",";
  payload += "\"raw\":{";
  payload += "\"T\":";
  payload += String(simState.temperature, 2);
  payload += ",\"P\":";
  payload += String(simState.pressure, 2);
  payload += "}";
  payload += "},";
  payload += "\"neo6m\":{";
  payload += "\"timestamp\":\"";
  payload += gpsTimestamp;
  payload += "\",";
  payload += "\"latitude\":";
  payload += String(simState.latitude, 5);
  payload += ",\"longitude\":";
  payload += String(simState.longitude, 5);
  payload += ",\"altitude\":";
  payload += String(simState.altitude, 1);
  payload += ",\"fix_time\":\"";
  payload += gpsFixTime;
  payload += "\",";
  payload += "\"raw\":\"$GPGGA,000000.00,0000.0000,N,00000.0000,E,1,08,0.9,000.0,M,0.0,M,,*00\"";
  payload += "}";
  payload += "}";
  payload += "}";

  return payload;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  randomSeed(analogRead(A0));
  Serial.println(F("[Sim RX] Iniciando receptor simulado"));
}

void loop() {
  const uint32_t now = millis();
  if (now - lastSimMillis >= SIM_PERIOD_MS) {
    const String telemetry = buildTelemetryPayload();
    Serial.println(telemetry);
    lastSimMillis = now;
  }
}
