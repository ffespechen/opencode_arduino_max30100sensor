#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

#include "config.h"

// =============================================================================
//  EcgDisplay
//
//  Dibuja el gráfico estilo electrocardiograma en la pantalla redonda GC9A01.
//  Mantiene un buffer circular con la forma de onda y se encarga de la
//  retícula, el trazo y las métricas (BPM y SpO2).
//
//  La forma de onda es sintética (complejo P-QRS-T) pero se sincroniza con
//  los latidos reales detectados por el MAX30100 (pico R).
// =============================================================================
class EcgDisplay {
public:
  EcgDisplay();
  bool begin();

  // Marca el instante de un latido detectado (pico R del ECG).
  void onBeat(uint32_t nowMs);

  // Agrega una muestra a la forma de onda según la fase del ciclo cardíaco.
  void pushSample(uint32_t nowMs, uint32_t beatIntervalMs);

  // Redibuja el cuadro completo y lo envía a la pantalla.
  void render(float bpm, uint8_t spo2, bool finger, bool error);

private:
  static float gauss(float t, float mu, float sigma);
  static float ecgShape(float tMs, float intervalMs);

  void drawCenteredText(const char *text, int16_t cx, int16_t y);
  void drawGrid();
  void drawTrace();
  void drawMetrics(float bpm, uint8_t spo2, bool finger, bool error);

  Arduino_DataBus *bus_;
  Arduino_GFX *gfx_;
  Arduino_Canvas *canvas_;

  int16_t trace_[TRACE_LEN];  // buffer circular del trazo (valor Y por columna)
  int16_t head_;              // índice de la muestra más reciente
  uint32_t lastBeatMs_;       // instante del último latido (pico R)
};
