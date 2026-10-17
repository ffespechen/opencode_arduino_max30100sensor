#pragma once

// =============================================================================
//  Configuración de pines y parámetros del monitor de pulso cardíaco MAX30100
//  con pantalla OLED redonda GC9A01.
//
//  Cambia aquí los pines o constantes sin tocar el resto del código.
// =============================================================================

// ---------- Sensor MAX30100 (bus I2C) ----------
// El ESP32 usa por defecto SDA=GPIO21 y SCL=GPIO22 para el I2C.
#define PIN_SDA               21
#define PIN_SCL               22
#define I2C_CLOCK_HZ          400000UL

// ---------- Pantalla GC9A01 (bus SPI) ----------
// El pin MISO del SPI no se utiliza (la pantalla solo recibe datos).
#define PIN_GC9A01_SCK        18
#define PIN_GC9A01_MOSI       23
#define PIN_GC9A01_CS         5
#define PIN_GC9A01_DC         16
#define PIN_GC9A01_RST        17

// ---------- LEDs indicadores ----------
#define PIN_LED_HEARTBEAT     25   // verde: destella con cada latido
#define PIN_LED_FINGER        26   // azul:  dedo colocado / lectura OK
#define PIN_LED_ERROR         27   // rojo:  sensor no detectado / error

// ---------- Pantalla y gráfico ----------
#define SCREEN_SIZE           240   // resolución de la pantalla (px)
#define TRACE_LEN             240   // columnas del buffer circular del trazo
#define TRACE_BASE_Y          120   // línea base (eje Y) del trazo ECG
#define TRACE_AMP             44    // amplitud máxima del trazo (px)

// ---------- Tiempos (ms) ----------
#define TRACE_TICK_MS         16    // periodo de muestreo del trazo
#define RENDER_MS             33    // periodo de refresco de pantalla (~30 fps)
#define BEAT_LED_MS           80    // duración del destello del LED de latido
#define NO_FINGER_TIMEOUT_MS  3000  // sin latidos por este tiempo => dedo retirado

// ---------- Colores (formato RGB565) ----------
#define COLOR_BG        0x0000   // negro (fondo)
#define COLOR_GRID      0x18E3   // gris verdoso tenue (retícula)
#define COLOR_TRACE     0x07E0   // verde brillante (trazo ECG)
#define COLOR_BPM       0x07E0   // verde (número de BPM)
#define COLOR_SPO2      0x07FF   // cian (valor de SpO2)
#define COLOR_DIM       0x9CD3   // gris claro (etiquetas)
#define COLOR_FINGER    0x07FF   // azul (indicador de dedo)
#define COLOR_ERROR     0xF800   // rojo (indicador de error)
#define COLOR_WARN      0xFDC0   // naranja (mensajes de aviso)
