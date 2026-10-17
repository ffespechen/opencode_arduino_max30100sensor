# Arquitectura y decisiones técnicas

> Documento complementario al `README.md`. Aquí se registran las observaciones
> más importantes surgidas durante la **planificación e implementación** del
> proyecto, así como las decisiones técnicas y sus motivos.

---

## 1. Hardware y conexiones

### 1.1 Resistencias pull-up del MAX30100 (I2C)

**Observación importante:** muchos módulos genéricos MAX30100 tienen las
resistencias *pull-up* del bus I2C colocadas en el **lado equivocado** de su
convertidor de nivel lógico. Esto hace que, aunque el cableado SDA/SCL parezca
correcto, el sensor **no sea detectado** (`pox.begin()` devuelve `false`).

**Solución adoptada:** se recomienda (y se documenta en el README) agregar **dos
resistencias de 4.7 kΩ** externas:

- `SDA ↔ 3.3V`
- `SCL ↔ 3.3V`

Este es el primer punto a revisar si aparece `MAX30100... FALLO` en el monitor
serie.

### 1.2 Pines del ESP32 elegidos

Al seleccionar los GPIO se evitaron, en lo posible, los pines problemáticos del
ESP32 DevKit:

- **Pines de la flash (GPIO 6–11):** no se usan.
- **Pines de *strapping* (0, 2, 5, 12, 15):** se evitó su uso para señales
  críticas. `CS` quedó en GPIO 5 (puede funcionar porque la línea CS del módulo
  suele tener pull-up propio), pero el resto de señales SPI usa pines libres.
- **I2C por defecto:** SDA = GPIO 21, SCL = GPIO 22 (los valores por defecto del
  ESP32), de modo que incluso si la librería llama a `Wire.begin()` sin
  argumentos, el bus queda en los pines correctos.

| Función | Pin | Nota |
|---|---|---|
| SDA (MAX30100) | GPIO 21 | I2C por defecto |
| SCL (MAX30100) | GPIO 22 | I2C por defecto |
| SCK (GC9A01) | GPIO 18 | SPI VSPI por defecto |
| MOSI (GC9A01) | GPIO 23 | SPI VSPI por defecto |
| CS (GC9A01) | GPIO 5 | |
| DC (GC9A01) | GPIO 16 | |
| RST (GC9A01) | GPIO 17 | |
| LED latido | GPIO 25 | verde |
| LED dedo | GPIO 26 | azul |
| LED error | GPIO 27 | rojo |

### 1.3 Alimentación

Tanto el MAX30100 como la pantalla GC9A01 trabajan con **lógica de 3.3V**, por
lo que se alimentan desde el pin de 3.3V del ESP32. No se requiere adaptador de
nivel lógico (a diferencia de lo que ocurriría con un Arduino UNO/Nano de 5V).

---

## 2. Elección del trazo del pulso (gráfico ECG)

Este fue uno de los puntos de decisión más importantes del diseño. Se evaluaron
dos enfoques:

### 2.1 Opción A — Onda PPG cruda (lectura directa del FIFO)

- Leer las muestras crudas de luz infrarroja directamente desde el FIFO del
  MAX30100 usando el driver de bajo nivel (`MAX30100::getRawIR()`).
- **Ventaja:** es la señal "real" del sensor (fotopletismograma).
- **Desventajas:**
  - Requiere implementar **detección de picos** propia para calcular la
    frecuencia cardíaca (o duplicar la lectura del FIFO junto con la librería de
    alto nivel, lo cual es conflictivo).
  - Mayor complejidad y más puntos de falla.
  - La forma PPG no se parece exactamente a un "electrocardiograma" clásico.

### 2.2 Opción B — Onda ECG sintética sincronizada con los latidos reales ✔️

**Elección adoptada.**

- Se usa `PulseOximeter` (librería `MAX30100lib`) solo para obtener **BPM y
  SpO₂ reales** (algoritmo robusto y probado).
- El trazo que se dibuja es un **complejo P-QRS-T sintético** generado por
  campanas de Gauss.
- El *callback* `onBeatDetected()` marca el **pico R**, de modo que el trazo
  **se sincroniza con cada latido real** detectado.

**Resultado:** se obtiene un gráfico visualmente idéntico a un ECG, con valores
de BPM/SpO₂ fiables, sin lidiar con el procesamiento crudo del FIFO.

### 2.3 Fase vs. tiempo absoluto en la forma de onda

Dentro de la Opción B hubo una segunda decisión: modelar la onda por **fracción
de fase del ciclo** (`p = t / intervalo`) o por **tiempo absoluto (ms) referido
al pico R**.

- Con **fase** (fracción del período), el ancho del complejo QRS se vuelve
  proporcional al período cardíaco: a frecuencias altas el pico R se hace tan
  angosto que puede "perderse" entre muestras (aliasing).
- Con **tiempo absoluto (ms)** se fijan centros y anchos en milisegundos, por lo
  que el QRS se mantiene **nítido y de ancho constante** sin importar el BPM.

**Elección adoptada:** tiempos absolutos en milisegundos. El pico R se define en
`t = 0` (y se "envuelve" en `t = intervalo`), la onda P poco antes del siguiente
latido, la S y la T después del pico.

---

## 3. Observaciones sobre las versiones de librerías

### 3.1 Arduino_GFX: la versión más reciente NO compila

- La versión **1.6.9** de `moononournation/GFX Library for Arduino` incluye
  `#include "esp32-hal-periman.h"`, un encabezado que **solo existe en el core
  Arduino-ESP32 3.x**.
- PlatformIO, con el platform `espressif32` por defecto, instala el core
  **2.0.5** (`framework-arduinoespressif32 @ 3.20005.220925`), que **no** tiene
  ese encabezado.
- **Resultado:** error de compilación
  `fatal error: esp32-hal-periman.h: No such file or directory`.

**Solución adoptada:** anclar `Arduino_GFX` a la versión **1.4.7**, que:

- Es compatible con el core 2.0.5.
- Mantiene la misma API para todo lo que usa el proyecto (`Arduino_ESP32SPI`,
  `Arduino_GC9A01`, `Arduino_Canvas`, `setTextSize`, etc.).

### 3.2 `printCenterText` no existe en Arduino_GFX 1.4.7

- El método `printCenterText()` (muy útil para centrar texto) **no existe** en la
  versión 1.4.7; se agregó en versiones posteriores.
- **Solución adoptada:** se implementó un helper propio `drawCenteredText()` que
  calcula el ancho del texto con `getTextBounds()` y luego posiciona el cursor
  centrado con `setCursor(cx - w/2, y)`.

### 3.3 MAX30100lib (oxullo) — API confirmada

Se verificó contra el código fuente de `oxullo/MAX30100lib @ 1.2.1`:

- Clase `PulseOximeter` con `begin()`, `update()`, `getHeartRate()` (float),
  `getSpO2()` (uint8_t) y `setOnBeatDetectedCallback()`.
- El enum de corriente del LED es `LEDCurrent` con valores
  `MAX30100_LED_CURR_*` (ej. `MAX30100_LED_CURR_7_6MA`).
- `setIRLedCurrent()` recibe un `LEDCurrent` (no `LedCurrent`, como aparece en
  algunos tutoriales desactualizados).

---

## 4. Observaciones sobre el código y el entorno

### 4.1 Formateo de flotantes (`%f`) con newlib-nano

- En el ESP32 (Arduino core 2.x) se usa **newlib-nano**, que **no siempre
  soporta `%f`** en `snprintf`/`sprintf`.
- **Solución adoptada:** para el número de BPM se usa la **parte entera**
  (`(int)(bpm + 0.5f)`) y `%d`, evitando por completo el formateo de flotantes.
  El resto de valores (SpO₂) ya es entero.
- Para depurar por puerto serie se usa `Serial.print(bpm, 0)`, que sí maneja
  flotantes correctamente (no pasa por `printf`).

### 4.2 Framebuffer en RAM (Arduino_Canvas)

- Para evitar el parpadeo típico de redibujar directamente sobre el TFT, se
  dibuja todo en un **framebuffer en RAM** (`Arduino_Canvas` de 240×240×2 bytes
  ≈ **112 KB**) y luego se envía a la pantalla con `flush()`.
- **Observación:** el consumo de RAM **estático** reportado por PlatformIO es
  bajo (≈5%), porque el framebuffer se asigna **en el heap** (`malloc`) en tiempo
  de ejecución. El ESP32 (320 KB de SRAM) tiene espacio suficiente, pero conviene
  saber que se reservan ~112 KB de heap.

### 4.3 Inicialización del bus I2C

- `PulseOximeter::begin()` internamente inicializa `Wire`; aun así, en el
  proyecto se llama explícitamente a `Wire.begin(PIN_SDA, PIN_SCL)` y
  `Wire.setClock(400000)` antes, para fijar pines y velocidad de forma
  determinista (en ESP32 los pines por defecto coinciden con los elegidos).

### 4.4 Velocidades de muestreo del trazo y del refresco

Se separaron dos cadencias independientes para un resultado fluido:

- **Muestreo del trazo:** cada `TRACE_TICK_MS = 16 ms` (≈62 muestras/s), lo que
  da una pantalla de ~3.8 s de recorrido.
- **Refresco de pantalla:** cada `RENDER_MS = 33 ms` (≈30 fps), suficiente para
  el trazo y sin saturar la CPU con `flush()`.

---

## 5. Resumen de decisiones clave

| # | Tema | Decisión | Motivo |
|---|---|---|---|
| 1 | Trazo del pulso | Onda P-QRS-T sintética sincronizada con latidos reales | Confiable, visual "ECG" y sin procesar el FIFO crudo |
| 2 | Forma de onda | Tiempos absolutos (ms) respecto del pico R | QRS nítido y de ancho constante a cualquier BPM |
| 3 | Librería de pantalla | `Arduino_GFX @ 1.4.7` | La 1.6.9 exige core ESP32 3.x; 1.4.7 compila con el core 2.0.5 por defecto |
| 4 | Texto centrado | Helper propio `drawCenteredText()` | `printCenterText()` no existe en 1.4.7 |
| 5 | Número BPM | Entero con `%d` | Evitar `%f` por newlib-nano en ESP32 |
| 6 | Sin parpadeo | Framebuffer `Arduino_Canvas` en RAM | Redibujo completo atómico, sin *tearing* |
| 7 | MAX30100 | Pull-ups externos 4.7 kΩ | Muchos módulos traen pull-ups en el lado equivocado |
| 8 | LEDs | 3 LEDs (latido, dedo, error) | Estado visible sin depender solo de la pantalla |

---

## 6. Posibles mejoras futuras

- **Onda PPG cruda real:** si se quisiera graficar la señal fotopletismográfica
  real en lugar de la sintética, habría que leer el FIFO con el driver de bajo
  nivel e implementar detección de picos propia (o combinar ambos en un esquema
  más complejo).
- **Interrupción INT del MAX30100:** actualmente se usa *polling* con
  `pox.update()`; podría usarse el pin INT para despertar la lectura.
- **Registro de datos (datalogging):** volcar BPM/SpO₂ a una microSD o vía
  serie/Bluetooth/WiFi para análisis posterior.
- **Migración al core ESP32 3.x:** permitiría usar Arduino_GFX más reciente y
  sus nuevas funciones; implicaría cambiar el `platform`/framework en
  `platformio.ini`.
