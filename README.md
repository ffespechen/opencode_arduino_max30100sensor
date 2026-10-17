# Monitor de Pulso Cardíaco y Oxímetro (MAX30100) con Gráfico ECG

Proyecto en **PlatformIO** (framework **Arduino**) que lee el sensor **MAX30100**
(pulso cardíaco y saturación de oxígeno SpO₂) y muestra las lecturas en una
**pantalla OLED redonda GC9A01 de 1.28"**, dibujando un trazo continuo estilo
**electrocardiograma (ECG)**. Además incluye **LEDs indicadores** de estado.

> ⚠️ **Aviso**: este dispositivo es de uso **educativo / de hobby**. No es un
> equipo médico y sus lecturas no deben usarse para diagnóstico clínico.

---

## Características

- Lectura de **frecuencia cardíaca (BPM)** y **saturación de oxígeno (SpO₂%)**
  con el sensor MAX30100 (bus I2C).
- Gráfico **tipo ECG** que se desplaza de forma continua por la pantalla redonda,
  con complejo P-QRS-T sincronizado con cada latido real detectado.
- **Retícula circular** estilo monitor cardíaco que aprovecha la forma de la
  pantalla.
- **3 LEDs indicadores**: latido, dedo colocado y error de sensor.
- Salida de depuración por **puerto serie** (115200 baudios).
- Renderizado sin parpadeo mediante un framebuffer en RAM.

---

## Lista de componentes

| # | Componente | Cantidad | Observaciones |
|---|---|---|---|
| 1 | Placa de desarrollo **ESP32 DevKit** (ESP32-WROOM-32) | 1 | Microcontrolador principal |
| 2 | Módulo sensor **MAX30100** (pulso + oxímetro) | 1 | Bus I2C, lógica 3.3V |
| 3 | Pantalla **OLED redonda GC9A01** 1.28" 240×240 | 1 | Interfaz SPI |
| 4 | LED 5 mm **verde** | 1 | Indicador de latido |
| 5 | LED 5 mm **azul** | 1 | Indicador de dedo / lectura OK |
| 6 | LED 5 mm **rojo** | 1 | Indicador de error |
| 7 | Resistencia **220 Ω** | 3 | En serie con cada LED |
| 8 | Resistencia **4.7 kΩ** | 2 | Pull-ups I2C (recomendadas, ver nota) |
| 9 | Cables dupont / protoboard | — | Para las conexiones |

---

## Diagrama de conexiones

### MAX30100 (bus I2C)

| MAX30100 | ESP32 DevKit |
|---|---|
| VIN | 3.3V |
| GND | GND |
| SCL | GPIO 22 |
| SDA | GPIO 21 |
| INT | (sin conectar) |

> **Nota importante sobre el MAX30100:** muchos módulos genéricos tienen las
> resistencias *pull-up* de I2C colocadas en el lado equivocado del convertidor
> de nivel lógico, lo que impide que el sensor sea detectado. La solución
> habitual es **agregar dos resistencias de 4.7 kΩ** entre **SDA ↔ 3.3V** y
> **SCL ↔ 3.3V**. Si `pox.begin()` falla, verifica esto primero.

### Pantalla GC9A01 1.28" (bus SPI)

| GC9A01 | ESP32 DevKit |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL / SCK | GPIO 18 |
| SDA / MOSI | GPIO 23 |
| RES / RST | GPIO 17 |
| DC | GPIO 16 |
| CS | GPIO 5 |
| BL / BLK (retroiluminación) | 3.3V |

> El pin **MISO** del ESP32 no se conecta: la pantalla solo recibe datos.

### LEDs indicadores

Cada LED se conecta con su **cátodo a GND** y su **ánodo a través de una
resistencia de 220 Ω** al pin correspondiente:

| LED | Color | GPIO ESP32 |
|---|---|---|
| Latido | Verde | GPIO 25 |
| Dedo / OK | Azul | GPIO 26 |
| Error | Rojo | GPIO 27 |

### Esquema resumido (ASCII)

```
        MAX30100                         ESP32 DevKit
        ┌─────────┐                      ┌──────────────┐
        │ VIN ────┼──────────────────────┤ 3.3V         │
        │ GND ────┼──────────────────────┤ GND          │
        │ SCL ────┼──────────────────────┤ GPIO 22      │
        │ SDA ────┼──────────────────────┤ GPIO 21      │
        │ INT     │  (sin conectar)      │              │
        └─────────┘                      │              │
                                         │              │
        GC9A01                           │              │
        ┌─────────┐                      │              │
        │ VCC ────┼──────────────────────┤ 3.3V         │
        │ GND ────┼──────────────────────┤ GND          │
        │ SCK ────┼──────────────────────┤ GPIO 18      │
        │ MOSI ───┼──────────────────────┤ GPIO 23      │
        │ RST ────┼──────────────────────┤ GPIO 17      │
        │ DC  ────┼──────────────────────┤ GPIO 16      │
        │ CS  ────┼──────────────────────┤ GPIO 5       │
        │ BL  ────┼──────────────────────┤ 3.3V         │
        └─────────┘                      │              │
                                         │              │
        LED verde  (220Ω) ───────────────┤ GPIO 25      │
        LED azul   (220Ω) ───────────────┤ GPIO 26      │
        LED rojo   (220Ω) ───────────────┤ GPIO 27      │
                                         └──────────────┘
```

---

## Estructura del proyecto

```
opencode_arduino_max30100/
├── platformio.ini          # Configuración: placa, librerías y flags
├── README.md               # Este documento
├── include/
│   ├── config.h            # Pines, tiempos y colores (todo el cableado aquí)
│   └── ecg_display.h       # Declaración de la clase EcgDisplay
├── src/
│   ├── main.cpp            # setup() y loop(): sensor, LEDs y orquestación
│   └── ecg_display.cpp     # Dibujo del gráfico ECG (retícula, trazo, métricas)
├── lib/                    # (vacía) Librerías propias del proyecto
└── test/                   # (vacía) Pruebas unitarias
```

**Arquitectura**:

- `main.cpp` concentra la lógica de adquisición: inicializa el sensor y la
  pantalla, lee el sensor en cada iteración (`pox.update()`), maneja el
  *callback* de latido detectado y controla los LEDs.
- `EcgDisplay` (en `include/ecg_display.h` + `src/ecg_display.cpp`) aísla todo
  el renderizado. Mantiene un **buffer circular** con el valor Y del trazo por
  cada columna y dibuja sobre un **framebuffer en RAM** (`Arduino_Canvas`), que
  se envía a la pantalla de una sola vez para evitar parpadeo.
- `config.h` centraliza los pines y constantes: para cambiar el cableado solo se
  edita este archivo.

### Cómo se genera el trazo ECG

El trazo es un **complejo P-QRS-T sintético** generado por campanas de Gauss
centradas en tiempos fijos (en ms) respecto del **pico R**. El *callback*
`onBeatDetected()` de la librería del MAX30100 marca el pico R real, de modo que:

1. El **pico R** del trazo coincide con cada **latido real** detectado.
2. El ancho del complejo QRS se mantiene **nítido y constante** aunque cambie la
   frecuencia cardíaca.
3. El LED verde destella con cada latido.

---

## Instalación y uso

### Requisitos

- [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI `pio`).
- Placa ESP32 DevKit.

> **Nota técnica:** la librería `Arduino_GFX` está anclada a la versión `1.4.7`
> en `platformio.ini`. Las versiones más recientes (1.6.x) requieren el core
> Arduino-ESP32 **3.x** (`esp32-hal-periman.h`), mientras que PlatformIO instala
> por defecto el core **2.0.5**. Con `1.4.7` el proyecto compila sin necesidad de
> cambiar el core.

### Pasos

1. Clona o copia este proyecto en tu equipo.
2. Conecta el hardware según el [diagrama de conexiones](#diagrama-de-conexiones).
3. Compila y sube el firmware:

   ```bash
   pio run -t upload
   ```

4. Abre el monitor serie para ver la depuración:

   ```bash
   pio device monitor
   ```

### Uso

1. Enciende la placa. La pantalla muestra la retícula ECG y el estado del sensor.
2. **Coloca el dedo índice** sobre el LED/detector del MAX30100 (ejerciendo una
   presión suave y constante, sin moverte).
3. Al detectar el primer latido, el trazo comienza a dibujar el complejo P-QRS-T
   y el LED verde destella con cada pulso.
4. El LED **azul** se enciende mientras el dedo esté colocado; la pantalla muestra
   los valores de **BPM** y **SpO₂%**.
5. Si retiras el dedo, aparece el aviso **"COLOQUE EL DEDO"** y el LED azul se
   apaga.

### Indicadores

| Estado | LED verde | LED azul | LED rojo | Pantalla |
|---|---|---|---|---|
| Sensor OK, sin dedo | apagado | apagado | apagado | "COLOQUE EL DEDO" |
| Midiendo | destella por latido | encendido | apagado | trazo ECG + BPM + SpO₂ |
| Sensor no detectado | apagado | apagado | **encendido** | "SIN SENSOR" |

---

## Solución de problemas

| Síntoma | Causa probable | Solución |
|---|---|---|
| `MAX30100... FALLO` en el monitor serie | Sensor no responde en I2C | Verifica SDA/SCL y agrega **pull-ups de 4.7 kΩ** (ver nota del MAX30100) |
| Lecturas de BPM/SpO₂ en `--` | Dedo mal colocado o en movimiento | Apoya el dedo con presión suave y constante |
| Pantalla negra | Conexión SPI o alimentación | Revisa SCK/MOSI/CS/DC/RST y que VCC sea 3.3V |
| Pantalla se enciende pero no dibuja | Pin CS/DC/RST mal conectado | Revisa la tabla de conexiones |
| Trazo no aparece | No hay latidos detectados | Espera a detectar el primer pulso con el dedo colocado |

---

## Licencia y créditos

- Proyecto de ejemplo con fines educativos.
- Librerías utilizadas:
  - [MAX30100lib](https://github.com/oxullo/Arduino-MAX30100) (OXullo Intersecans) — GPL-3.0.
  - [Arduino_GFX](https://github.com/moononournation/Arduino_GFX) (Moon On Our Nation) — para la pantalla GC9A01.
