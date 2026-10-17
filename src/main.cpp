#include <Arduino.h>
#include <Wire.h>

#include "MAX30100_PulseOximeter.h"
#include "config.h"
#include "ecg_display.h"

// =============================================================================
//  Monitor de pulso cardíaco y oxímetro MAX30100
//  con gráfico estilo ECG en pantalla OLED redonda GC9A01.
//
//  - Lee el sensor MAX30100 (frecuencia cardíaca y SpO2).
//  - Dibuja un trazo ECG que se desplaza en la pantalla redonda.
//  - Enciende LEDs indicadores de estado (latido, dedo, error).
// =============================================================================

PulseOximeter pox;
EcgDisplay display;

volatile bool beatDetected = false;

bool sensorOk = false;
uint32_t lastBeatMs = 0;      // instante del último latido (detección de dedo)
uint32_t beatLedOffMs = 0;    // instante en que se apaga el LED de latido

// La librería invoca este callback cuando detecta un latido (pico R).
void onBeatDetected() {
  beatDetected = true;
}

void setup() {
  Serial.begin(115200);

  // ---- LEDs indicadores ----
  pinMode(PIN_LED_HEARTBEAT, OUTPUT);
  pinMode(PIN_LED_FINGER, OUTPUT);
  pinMode(PIN_LED_ERROR, OUTPUT);
  digitalWrite(PIN_LED_HEARTBEAT, LOW);
  digitalWrite(PIN_LED_FINGER, LOW);
  digitalWrite(PIN_LED_ERROR, LOW);

  // ---- Pantalla GC9A01 ----
  Serial.print("Iniciando pantalla GC9A01... ");
  if (display.begin()) {
    Serial.println("OK");
  } else {
    Serial.println("FALLO");
  }

  // ---- Sensor MAX30100 (I2C) ----
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  Serial.print("Iniciando MAX30100... ");
  sensorOk = pox.begin();
  if (sensorOk) {
    Serial.println("OK");
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
  } else {
    Serial.println("FALLO");
    Serial.println("Revisa el cableado I2C (SDA/SCL) y las resistencias pull-up de 4.7k.");
    digitalWrite(PIN_LED_ERROR, HIGH);
  }
}

void loop() {
  uint32_t now = millis();

  // ---- Lectura del sensor ----
  if (sensorOk) {
    pox.update();

    if (beatDetected) {
      beatDetected = false;
      lastBeatMs = now;
      display.onBeat(now);

      // Destello del LED de latido.
      digitalWrite(PIN_LED_HEARTBEAT, HIGH);
      beatLedOffMs = now + BEAT_LED_MS;
    }
  }

  // Apagar el LED de latido cuando termine el destello.
  if (beatLedOffMs != 0 && now >= beatLedOffMs) {
    beatLedOffMs = 0;
    digitalWrite(PIN_LED_HEARTBEAT, LOW);
  }

  // ---- Estado del dedo y de error ----
  bool finger = sensorOk && (lastBeatMs != 0) && (now - lastBeatMs < NO_FINGER_TIMEOUT_MS);
  bool error = !sensorOk;

  digitalWrite(PIN_LED_FINGER, finger ? HIGH : LOW);
  digitalWrite(PIN_LED_ERROR, error ? HIGH : LOW);

  // ---- Lecturas actuales ----
  float bpm = sensorOk ? pox.getHeartRate() : 0.0f;
  uint8_t spo2 = sensorOk ? pox.getSpO2() : 0;

  // Intervalo estimado del ciclo cardíaco (para la forma de onda).
  uint32_t beatInterval = 1000;  // 60 BPM por defecto
  if (bpm >= 30.0f && bpm <= 220.0f) {
    beatInterval = (uint32_t)(60000.0f / bpm);
  }
  if (beatInterval < 300) beatInterval = 300;    // máx. 200 BPM
  if (beatInterval > 2000) beatInterval = 2000;  // mín. 30 BPM

  // ---- Avanzar el trazo a cadencia fija ----
  static uint32_t lastTickMs = 0;
  if (now - lastTickMs >= TRACE_TICK_MS) {
    lastTickMs = now;
    display.pushSample(now, beatInterval);
  }

  // ---- Refrescar la pantalla ----
  static uint32_t lastRenderMs = 0;
  if (now - lastRenderMs >= RENDER_MS) {
    lastRenderMs = now;
    display.render(bpm, spo2, finger, error);
  }

  // ---- Depuración por serial (1 vez por segundo) ----
  static uint32_t lastReportMs = 0;
  if (now - lastReportMs >= 1000) {
    lastReportMs = now;
    Serial.print("BPM: ");
    Serial.print(bpm, 0);
    Serial.print("  SpO2: ");
    Serial.print(spo2);
    Serial.print("%  dedo: ");
    Serial.println(finger ? "SI" : "NO");
  }
}
