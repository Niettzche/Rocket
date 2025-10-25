// Wiring reference (adjust pins to match your board / module):
//   SPI MOSI -> 11 (Arduino Uno) / 23 (ESP32)
//   SPI MISO -> 12 (Arduino Uno) / 19 (ESP32)
//   SPI SCK  -> 13 (Arduino Uno) / 18 (ESP32)
//   LoRa NSS (CS) -> D10 (Arduino Uno) / 5 (ESP32)
//   LoRa RST      -> D9  (Arduino Uno) / 14 (ESP32)
//   LoRa DIO0     -> D2  (Arduino Uno) / 26 (ESP32)
//   GND and 3V3 (o 5V si el módulo lo soporta) conectados al módulo SX127x.
// Update the pins below with LoRa.setPins(csPin, resetPin, dio0Pin) if your layout differs.
#include <SPI.h>
#include <LoRa.h>

constexpr long LORA_FREQ_HZ = 433E6;
constexpr uint8_t LORA_SF = 7;
constexpr uint32_t FRAME_TIMEOUT_MS = 2500;
constexpr uint8_t MAX_FRAMES = 8;
constexpr uint8_t HEADER_MAGIC = 'J';

struct FrameBucket {
  String topic;
  uint8_t total = 0;
  uint8_t received = 0;
  String parts[MAX_FRAMES];
  bool seen[MAX_FRAMES];
  uint32_t timestamp = 0;
};

FrameBucket bucket;

void resetBucket() {
  bucket.topic = "";
  bucket.total = 0;
  bucket.received = 0;
  bucket.timestamp = 0;
  for (uint8_t i = 0; i < MAX_FRAMES; ++i) {
    bucket.parts[i] = "";
    bucket.seen[i] = false;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println(F("[LoRa RX] Iniciando..."));
  LoRa.setPins(10, 9, 2);  // NSS, RST, DIO0 (update if you use different wiring)
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println(F("[LoRa RX] No se pudo inicializar el radio"));
    while (true) {
      delay(1000);
    }
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  resetBucket();

  Serial.print(F("[LoRa RX] Escuchando @ "));
  Serial.print(LORA_FREQ_HZ / 1e6);
  Serial.println(F(" MHz"));
}

void handleFrame(const uint8_t *data, int len) {
  if (len < 5 || data[0] != HEADER_MAGIC) {
    Serial.println(F("[LoRa RX] Frame inválido (cabecera)"));
    return;
  }

  uint8_t topicLen = data[1];
  if (topicLen > 15) {
    topicLen = 15;
  }
  if (2 + topicLen + 2 > len) {
    Serial.println(F("[LoRa RX] Frame truncado"));
    return;
  }

  String topic = "";
  for (uint8_t i = 0; i < topicLen; ++i) {
    topic += char(data[2 + i]);
  }
  if (topic.length() == 0) {
    topic = F("sensors");
  }

  uint8_t index = data[2 + topicLen];
  uint8_t total = data[3 + topicLen];
  if (index < 1) {
    index = 1;
  }
  if (total < 1) {
    total = 1;
  }
  if (total > MAX_FRAMES) {
    Serial.println(F("[LoRa RX] Demasiados fragmentos; aumenta MAX_FRAMES"));
    return;
  }
  int payloadOffset = 4 + topicLen;
  int payloadLen = len - payloadOffset;
  if (payloadLen <= 0) {
    Serial.println(F("[LoRa RX] Sin payload"));
    return;
  }

  const uint32_t now = millis();
  if (bucket.topic != topic || bucket.total != total ||
      (bucket.timestamp > 0 && now - bucket.timestamp > FRAME_TIMEOUT_MS)) {
    resetBucket();
    bucket.topic = topic;
    bucket.total = total;
    bucket.timestamp = now;
  }

  uint8_t slot = index - 1;
  if (slot >= MAX_FRAMES) {
    Serial.println(F("[LoRa RX] Índice fuera de rango"));
    return;
  }

  if (!bucket.seen[slot]) {
    bucket.parts[slot] = "";
    for (int i = 0; i < payloadLen; ++i) {
      bucket.parts[slot] += char(data[payloadOffset + i]);
    }
    bucket.seen[slot] = true;
    bucket.received++;
  }

  if (bucket.received == bucket.total) {
    String assembled = "";
    for (uint8_t i = 0; i < bucket.total; ++i) {
      assembled += bucket.parts[i];
    }
    Serial.println(F("===== Payload recibido ====="));
    Serial.print(F("Topic: "));
    Serial.println(bucket.topic);
    Serial.println(assembled);
    Serial.println(F("============================"));

    resetBucket();
  } else {
    Serial.print(F("[LoRa RX] Fragmento "));
    Serial.print(index);
    Serial.print('/');
    Serial.print(total);
    Serial.println(F(" almacenado"));
  }
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  uint8_t buffer[255];
  int len = 0;
  while (LoRa.available() && len < sizeof(buffer)) {
    buffer[len++] = (uint8_t)LoRa.read();
  }

  long rssi = LoRa.packetRssi();
  float snr = LoRa.packetSnr();
  Serial.print(F("[LoRa RX] Paquete "));
  Serial.print(len);
  Serial.print(F(" B (RSSI "));
  Serial.print(rssi);
  Serial.print(F(" dBm, SNR "));
  Serial.print(snr, 1);
  Serial.println(F(" dB)"));

  handleFrame(buffer, len);
}
