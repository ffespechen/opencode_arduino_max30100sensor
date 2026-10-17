#include "ecg_display.h"

#include <math.h>
#include <stdio.h>

// =============================================================================
//  Forma de onda ECG sintética (complejo P-QRS-T)
// =============================================================================

// Campana de Gauss centrada en `mu` con desviación `sigma`.
float EcgDisplay::gauss(float t, float mu, float sigma) {
  float d = t - mu;
  return expf(-(d * d) / (2.0f * sigma * sigma));
}

// Devuelve la amplitud normalizada de la onda (aprox. [-0.35, 1.0]) para un
// instante `tMs` dentro del ciclo cardíaco de duración `intervalMs`.
//
// Los tiempos (en ms) están referidos al pico R (t = 0), que coincide con el
// latido detectado. De esta forma el ancho del complejo QRS se mantiene
// constante y nítido aunque varíe la frecuencia cardíaca.
float EcgDisplay::ecgShape(float tMs, float intervalMs) {
  float y = 0.0f;

  // Onda P (justo antes del siguiente pico R)
  y += 0.22f * gauss(tMs, intervalMs - 110.0f, 22.0f);

  // Onda Q (pequeña depresión previa al pico R)
  y += -0.12f * gauss(tMs, 12.0f, 6.0f);

  // Onda R (pico principal; se "envuelve" en t=0 y t=intervalo)
  y += 1.00f * (gauss(tMs, 0.0f, 10.0f) + gauss(tMs, intervalMs, 10.0f));

  // Onda S (depresión posterior al pico R)
  y += -0.22f * gauss(tMs, 34.0f, 12.0f);

  // Onda T (repolarización ventricular)
  y += 0.30f * gauss(tMs, 210.0f, 50.0f);

  return y;
}

// =============================================================================
//  Ciclo de vida
// =============================================================================

EcgDisplay::EcgDisplay() {
  bus_ = new Arduino_ESP32SPI(
      PIN_GC9A01_DC, PIN_GC9A01_CS, PIN_GC9A01_SCK, PIN_GC9A01_MOSI,
      GFX_NOT_DEFINED /* MISO no conectado */);
  gfx_ = new Arduino_GC9A01(bus_, PIN_GC9A01_RST, 0 /* rotación */);
  canvas_ = new Arduino_Canvas(SCREEN_SIZE, SCREEN_SIZE, gfx_);

  head_ = 0;
  lastBeatMs_ = 0;
  for (int i = 0; i < TRACE_LEN; i++) {
    trace_[i] = TRACE_BASE_Y;
  }
}

bool EcgDisplay::begin() {
  if (!gfx_->begin()) {
    return false;
  }
  canvas_->begin();
  canvas_->fillScreen(COLOR_BG);
  canvas_->flush();
  return true;
}

void EcgDisplay::onBeat(uint32_t nowMs) {
  lastBeatMs_ = nowMs;
}

void EcgDisplay::pushSample(uint32_t nowMs, uint32_t beatIntervalMs) {
  if (beatIntervalMs == 0) {
    beatIntervalMs = 1000;  // 60 BPM por defecto
  }

  // Fase del ciclo cardíaco dentro de [0, intervalo).
  uint32_t sinceBeat = (nowMs >= lastBeatMs_) ? (nowMs - lastBeatMs_) : 0;
  uint32_t t = sinceBeat % beatIntervalMs;

  float v = ecgShape((float)t, (float)beatIntervalMs);
  int16_t y = (int16_t)(TRACE_BASE_Y - v * TRACE_AMP);

  head_ = (head_ + 1) % TRACE_LEN;
  trace_[head_] = y;
}

void EcgDisplay::render(float bpm, uint8_t spo2, bool finger, bool error) {
  // Se dibuja todo en un framebuffer en RAM y luego se envía a la pantalla:
  // evita el parpadeo típico de redibujar directamente sobre el TFT.
  canvas_->fillScreen(COLOR_BG);
  drawGrid();
  drawTrace();
  drawMetrics(bpm, spo2, finger, error);
  canvas_->flush();
}

// =============================================================================
//  Dibujo
// =============================================================================

// Dibuja `text` centrado horizontalmente en `cx`, con el borde superior en `y`.
// (Equivalente a printCenterText, no disponible en Arduino_GFX 1.4.7.)
void EcgDisplay::drawCenteredText(const char *text, int16_t cx, int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  canvas_->setCursor(cx - (int16_t)(w / 2), y);
  canvas_->print(text);
}

void EcgDisplay::drawGrid() {
  int16_t cx = SCREEN_SIZE / 2;
  int16_t cy = SCREEN_SIZE / 2;

  // Anillo exterior y borde interior.
  canvas_->drawCircle(cx, cy, 119, COLOR_GRID);
  canvas_->drawCircle(cx, cy, 108, COLOR_GRID);

  // Guías concéntricas tenues.
  canvas_->drawCircle(cx, cy, 70, COLOR_GRID);
  canvas_->drawCircle(cx, cy, 40, COLOR_GRID);

  // "Papel" del ECG: líneas horizontales dentro de la banda del trazo.
  for (int16_t y = TRACE_BASE_Y - TRACE_AMP; y <= TRACE_BASE_Y + TRACE_AMP;
       y += TRACE_AMP / 2) {
    canvas_->drawFastHLine(0, y, SCREEN_SIZE, COLOR_GRID);
  }

  // Líneas verticales (cuadrícula de tiempo).
  for (int16_t x = 0; x < SCREEN_SIZE; x += 30) {
    canvas_->drawFastVLine(x, TRACE_BASE_Y - TRACE_AMP, TRACE_AMP * 2, COLOR_GRID);
  }
}

void EcgDisplay::drawTrace() {
  // Se recorre el buffer circular desde la muestra más antigua (x=0) hasta la
  // más reciente (x=TRACE_LEN-1).
  for (int16_t x = 0; x < TRACE_LEN - 1; x++) {
    int16_t i0 = (head_ + 1 + x) % TRACE_LEN;
    int16_t i1 = (head_ + 2 + x) % TRACE_LEN;
    canvas_->drawLine(x, trace_[i0], x + 1, trace_[i1], COLOR_TRACE);
  }

  // Punto brillante en el extremo derecho (muestra más reciente).
  canvas_->fillCircle(TRACE_LEN - 1, trace_[head_], 2, COLOR_BPM);
}

void EcgDisplay::drawMetrics(float bpm, uint8_t spo2, bool finger, bool error) {
  int16_t cx = SCREEN_SIZE / 2;
  char buf[16];

  // ---- Frecuencia cardíaca (BPM) ----
  canvas_->setTextColor(COLOR_DIM);
  canvas_->setTextSize(1);
  drawCenteredText("BPM", cx, 26);

  if (bpm >= 1.0f && bpm < 250.0f) {
    // Se usa la parte entera para evitar el formateo de flotantes (%f),
    // que newlib-nano no siempre soporta en el ESP32.
    snprintf(buf, sizeof(buf), "%d", (int)(bpm + 0.5f));
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  canvas_->setTextColor(COLOR_BPM, COLOR_BG);
  canvas_->setTextSize(4);
  drawCenteredText(buf, cx, 42);

  // ---- Saturación de oxígeno (SpO2) ----
  canvas_->setTextColor(COLOR_DIM);
  canvas_->setTextSize(1);
  drawCenteredText("SpO2", cx, 190);

  if (spo2 > 0 && spo2 <= 100) {
    snprintf(buf, sizeof(buf), "%u%%", spo2);
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  canvas_->setTextColor(COLOR_SPO2, COLOR_BG);
  canvas_->setTextSize(2);
  drawCenteredText(buf, cx, 202);

  // ---- Indicadores de estado (puntos que reflejan los LEDs) ----
  int16_t dotY = 14;
  canvas_->fillCircle(cx - 16, dotY, 4, finger ? COLOR_FINGER : COLOR_BG);
  canvas_->drawCircle(cx - 16, dotY, 4, COLOR_GRID);

  canvas_->fillCircle(cx + 16, dotY, 4, error ? COLOR_ERROR : COLOR_BG);
  canvas_->drawCircle(cx + 16, dotY, 4, COLOR_GRID);

  // ---- Mensajes de estado ----
  canvas_->setTextColor(COLOR_WARN, COLOR_BG);
  canvas_->setTextSize(2);
  if (error) {
    drawCenteredText("SIN SENSOR", cx, TRACE_BASE_Y - 10);
  } else if (!finger) {
    drawCenteredText("COLOQUE EL DEDO", cx, TRACE_BASE_Y - 10);
  }
}
