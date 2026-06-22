# SAQ Air Quality

Firmware embebido en **C++** para **Particle Photon** (ARM Cortex-M3, **sin FPU**) que convierte un microcontrolador de 128 KB de RAM en una estación de calidad del aire con DSP de audio en tiempo real: muestrea el micrófono por **ISR a 16 kHz**, detecta **palmas** (FSM con umbral adaptativo μ + K·σ) y **silbidos** (algoritmo **Goertzel** en aritmética entera Q15), agrega un **AQI estándar EPA** (con NowCast) sobre media docena de sensores ambientales y publica todo por **MQTT** con auto-discovery de Home Assistant — todo bajo un **scheduler cooperativo no bloqueante, sin RTOS y con cero asignación de heap**.

> Proyecto personal de **mdps**: el foco está en la ingeniería de firmware — planificación determinista, pipeline de audio por interrupción con buffering multi-etapa, DSP en punto fijo y optimización agresiva del footprint en flash/RAM.

---

## Lo más destacable (de un vistazo)

- **DSP de audio en tiempo real sin FPU.** Captura por ISR a 16 kHz en buffers circulares; el ISR solo escribe y el procesado vive en el loop principal. **Cuádruple buffer** (512 muestras/bloque ≈ 32 ms) que tolera ~96 ms de jitter del loop antes de perder datos, con contador de bloques descartados para diagnóstico.
- **Detección de palmas (FSM).** Envolvente (rectificación + IIR), **umbral adaptativo μ + K·σ** con *floor* de σ, **Schmitt trigger** (histéresis) y una máquina de estados (IDLE→RISING→PEAK→FALLING→GAP) con validación temporal que cuenta secuencias de 1–4 palmas y reporta SNR.
- **Detección de silbidos (Goertzel).** Banco de *bins* tonales entre `FMIN`–`FMAX` resuelto con coeficientes **Q15 (enteros)** — apto para un núcleo sin FPU — con criterios de tonalidad, estabilidad de frecuencia y duración mínima, más *guard* anti-falsos tras una palma.
- **Motor AQI estándar EPA.** Agregación en cubos minuto→hora→día con truncado por contaminante y *breakpoints* EPA (PM2.5 y PM10), variante **NowCast** (buffer circular de 12 h) y callbacks al cerrar cada cubo.
- **Cero heap, footprint optimizado.** Sin `malloc`/`new` en caliente (almacenamiento estático + *placement new*), filtros EMA en punto flotante ligero, *subsetting* de fuentes y *flags* de compilación por feature para encajar en la flash (~13 KB recuperados en una pasada de optimización).
- **Robustez de campo.** Persistencia EEPROM **versionada con checksum FNV-1a** y escritura *lazy* (anti-desgaste), watchdog hardware (IWDG), reconexión MQTT con backoff y pausa de tareas durante OTA.

---

## Hardware

### Plataforma
- **Particle Photon** (ARM Cortex-M3, 1 MB Flash, 128 KB RAM)
- Particle OS con `SYSTEM_THREAD(ENABLED)`

### Sensores
| Sensor | Parámetros | Interfaz |
|--------|-----------|----------|
| CCS811 | CO2 (ppm), TVOC (ppb) | I2C |
| BME280 | Temperatura, Humedad, Presión | I2C |
| BMP280 | Temperatura, Presión | I2C |
| PMS5003/5003ST | PM1.0, PM2.5, PM10 (+HCHO, T, H en ST) | UART |
| BH1750 | Luminosidad (lux) | I2C |
| ADC interno | Nivel de ruido (dBFS) | ADC @ 16 kHz |

### Salidas
- Display OLED 128×64 (SSD1306)
- NeoPixel RGB LED
- Buzzer (tonos RTTTL)

---

## Arquitectura

### Scheduler cooperativo (sin RTOS)
El loop principal corre una tabla de tareas, cada una con su período (ms) y dos *flags*: `catchup` (re-planifica sobre el período exacto, sin deriva) y `run_if_flashing` (qué tareas siguen vivas durante OTA). Cada `loop()` dispone de un presupuesto de tiempo (`budget_ms`) para repartir CPU de forma justa. No hay bloqueos largos: ninguna tarea cede el control con `delay()`. Resultado: comportamiento determinista en un solo hilo, sin la sobrecarga ni la RAM de un RTOS.

### Audio — captura por ISR + cuádruple buffer
`SparkIntervalTimer` dispara un ISR a **16 kHz** que lee el ADC interno y lo escribe en un anillo de **4 buffers** de 512 muestras (≈32 ms/bloque). El ISR *solo* produce; el consumidor (loop principal) procesa el bloque completo. Con 4 buffers el sistema absorbe ~96 ms de jitter (I2C del display, ráfagas MQTT) antes de descartar datos, y los descartes se contabilizan para diagnóstico. El ADC se configura a 112 ciclos para muestrear señal de audio con margen temporal real.

### DSP: palmas (FSM) y silbidos (Goertzel)
La envolvente de la señal se obtiene por rectificación + IIR exponencial. Un **umbral adaptativo `μ + K·σ`** (con *floor* de σ y **Schmitt trigger** para histéresis) alimenta una **FSM** (IDLE→RISING→PEAK→FALLING→GAP) que valida duración de pico, separación entre palmas y *timeouts* de secuencia (1–4 palmas), reportando recuento, pico y SNR. La detección de silbidos usa **Goertzel con coeficientes Q15 enteros** (sin FPU) sobre un banco de *bins* entre `FMIN`–`FMAX`, exigiendo tonalidad (potencia_pico/total), estabilidad de frecuencia y duración mínima, con *guard* anti-falsos tras una palma.

### Motor AQI (estándar EPA + NowCast)
Agrega concentraciones en cubos alineados **minuto → hora → día** con truncado por contaminante y *breakpoints* EPA (PM2.5 y PM10), más la variante **NowCast** sobre un buffer circular de 12 h. Cada nivel exige un mínimo de muestras válidas y emite un callback al cerrarse el cubo. Diseño sin asignación dinámica (2 canales fijos).

### MQTT + Home Assistant
- Publica telemetría de sensores en JSON construido sin asignación dinámica (`COMMS::JSON`)
- Auto-discovery de entidades en Home Assistant (con disponibilidad vinculada al LWT)
- Comandos entrantes enrutados por una tabla de *handlers*: LED, buzzer, offsets de calibración, calibración de micrófono, duty-cycle del PMS y comandos de sistema
- Reconexión con backoff y soporte opcional TLS

### Persistencia (EEPROM) versionada
Cabecera **versionada con checksum FNV-1a**: al arrancar valida la integridad y, si falla, cae a *defaults*. Escritura *lazy* (flag *dirty* + intervalo mínimo) para minimizar el desgaste de la flash emulada. Layout binario fijo (sin `#ifdef`) para garantizar orden estable entre builds. Guarda offsets de sensores, calibración de micrófono, baseline del CCS811 y config de LED/buzzer/PMS.

---

## Configuración multi-dispositivo

El archivo [config/config_device.h](src/config/config_device.h) selecciona qué dispositivo compilar:

```c
#include "config_device_3.h"   // ← cambiar el número según el dispositivo
```

Cada archivo `config_device_X.h` define los flags del hardware disponible y sus parámetros:

```c
#define ENABLE_PARTICLE_SENSOR          // Sensor de particulas PMS5003/5003ST/7003
#define ENABLE_LUX_SENSOR               // Sensor de luz BH1750
#define TEMP_SENSOR_BME280              // Sensor BME280 (vs BMP280)
#define ENABLE_NEOPIXEL                 // LED NeoPixel
#define ENABLE_BUZZER                   // Buzzer piezo (melodias RTTTL)
#define AUDIO_ENABLE_DETECT_CLAPS       // Deteccion de palmas (FSM)
#define AUDIO_ENABLE_WHISTLE_GOERTZEL   // Deteccion de silbidos (Goertzel)
#define HA_ENABLE_DISCOVERY             // Home Assistant auto-discovery
#define ENABLE_AQI                      // Motor AQI (EPA + NowCast)
```

Los parámetros globales (intervalos de polling, host MQTT, tamaños de buffer de audio, reglas AQI) se configuran en [config/config.h](src/config/config.h).

---

## Estructura del proyecto

```
src/
├── SAQAirQuality.ino         # Punto de entrada + scheduler de tareas
├── config/                     # Configuración global y por dispositivo
├── libs/                       # Librerías propias (AQI, audio, PMS, LED, JSON…)
├── utils/                      # Debugging, profiling, helpers
├── handlers/                   # Manejadores de comandos MQTT
├── sensors.h/cpp               # Lectura y promediado de sensores
├── sound.h/cpp                 # Procesamiento de audio + ISR
├── screen.h/cpp                # Renderizado del display OLED
├── comms.h/cpp                 # Gestión de conexión MQTT
├── comms_router.h/cpp          # Enrutado de mensajes MQTT
├── ha_discovery.h/cpp          # Auto-discovery para Home Assistant
└── persistence.h/cpp           # Almacenamiento EEPROM
```

### Dependencias externas

Este repositorio contiene únicamente el código propio. Las librerías de terceros no se
incluyen; instálalas con el gestor de librerías de Particle (o colócalas en `lib/`):

- `Adafruit_CCS811` (CO2/TVOC) — con extensiones `getBaseline()` / `setBaseline()`
- `Adafruit_BME280` / `Adafruit_BMP280` + `Adafruit_Sensor`
- `BH1750` (luminosidad)
- `neopixel` (LED RGB)
- `MQTT-TLS` (cliente MQTT con TLS)
- `SparkIntervalTimer` (muestreo de audio por ISR)

---

# MQTT Endpoints

## Referencia rapida

### Topics publicados (dispositivo → broker)

| Subtopic | Descripcion | Retained | Periodo/Trigger |
|----------|------------|----------|-----------------|
| `/LWT` | Estado de conexion (`Online`/`Offline`) | Si | Conectar/desconectar |
| `/tele/INFO` | Version firmware, IP, uptime | Si | Al conectar |
| `/tele/SENSOR` | Todos los sensores (T, H, P, CO2, TVOC, PM, lux) | No | ~90s |
| `/tele/SENSOR/NOISE` | Nivel de ruido (dBFS rapido + lento) | No | Adaptativo (2s-60s) |
| `/tele/SENSOR/CLAP` | Palmas detectadas (count, SNR, peak) | No | Al detectar |
| `/tele/SENSOR/WHISTLE` | Silbido detectado (freq, tonality) | No | Al detectar |
| `/tele/SENSOR/AQI/{p}/{s}` | Indice AQI por contaminante/periodo | No | Al cerrar cubo |
| `/tele/PMS_STATE` | Estado duty-cycle PMS | Si | Al cambiar estado |

### Topics de comando (broker → dispositivo)

| Subtopic | Respuesta | Descripcion |
|----------|-----------|-------------|
| `/cmd/LED` | `/stat/LED` | Control NeoPixel |
| `/cmd/BUZZER` | `/stat/BUZZER` | On/off buzzer |
| `/cmd/BUZZER/play` | `/stat/BUZZER` | Reproducir melodia RTTTL |
| `/cmd/OFFSETS` | `/stat/OFFSETS` | Fijar offsets de calibracion |
| `/cmd/OFFSETS/get` | `/stat/OFFSETS` | Leer offsets actuales |
| `/cmd/AUDIO_CAL` | `/stat/AUDIO_CAL` | Calibracion de microfono |
| `/cmd/PMS_CFG` | `/stat/PMS_CFG` | Configurar duty-cycle PMS |
| `/cmd/HA` | `/stat/HA` | Control HA auto-discovery |
| `/cmd/REBOOT` | `/stat/SYSTEM` | Reiniciar dispositivo |

---

## Configuración base

El topic base es específico por dispositivo (definido en `config_device_X.h`):

| Dispositivo | BASE_TOPIC | CLIENT_ID |
|-------------|-----------|-----------|
| SAQ1 | `homebot/SAQ-1` | `saq1` |
| SAQ2 | `homebot/SAQ-2` | `saq2` |
| SAQ3 | `homebot/SAQ-3` | `saq3` |
| SAQ4 | `homebot/SAQ-4` | `saq4` |
| Global | `homebot/saqs` | — |

---

## Topics publicados (dispositivo → broker)

### LWT — Estado de conexión
| Topic | `{BASE_TOPIC}/LWT` |
|-------|--------------------|
| Payload | `"Online"` / `"Offline"` (retained) |
| Cuándo | Al conectar / al desconectar |

---

### Telemetría de sensores
| Topic | `{BASE_TOPIC}/tele/SENSOR` |
|-------|--------------------------|
| Período | ~90 s |

```json
{
  "pm1_ugm3": 10,
  "pm25_ugm3": 25,
  "pm10_ugm3": 45,
  "hcho": 5,
  "lux": 1500.00,
  "temp_c": 22.50,
  "hum_percent": 45.50,
  "press_mb": 1013,
  "co2_ppm": 450,
  "tvoc_ppm": 25,
  "time": 1708345600
}
```

---

### Ruido ambiental
| Topic | `{BASE_TOPIC}/tele/SENSOR/NOISE` |
|-------|--------------------------------|
| Publicacion | Adaptativa: cuando cambia >= 2 dB (min 2s, max 60s heartbeat) |

```json
{
  "dbfs": -42.5,
  "dbfs_slow": -44.2,
  "time": 1708345600
}
```

- `dbfs`: nivel actual (IIR rapido, tau ~1.5s)
- `dbfs_slow`: nivel ambiente (IIR lento, tau ~45s)

---

### Detección de palmas (requiere `AUDIO_ENABLE_DETECT_CLAPS`)
| Topic | `{BASE_TOPIC}/tele/SENSOR/CLAP` |
|-------|-------------------------------|
| Cuándo | Al detectar una secuencia |

```json
{
  "count": 3,
  "period": 2000,
  "peak_dbfs": -20.5,
  "ambient_dbfs": -45.3,
  "snr_db": 24.8,
  "time": 1708345600
}
```

---

### Detección de silbidos (requiere `AUDIO_ENABLE_WHISTLE_GOERTZEL`)
| Topic | `{BASE_TOPIC}/tele/SENSOR/WHISTLE` |
|-------|----------------------------------|
| Cuándo | Al detectar un silbido |

```json
{
  "type": "whistle",
  "duration_ms": 850,
  "freq_hz": 2500,
  "tonality": 0.92,
  "level_dbfs": -22.5
}
```

---

### AQI (requiere `ENABLE_AQI`)
| Topic | `{BASE_TOPIC}/tele/SENSOR/AQI/{pollutant}/{scope}` |
|-------|--------------------------------------------------|
| Cuándo | Al cerrarse cada cubo de agregación |

`pollutant`: `pm25` / `pm10`
`scope`: `minute` / `hour` / `day` / `nowcast`

```json
{
  "pollutant": "pm25",
  "scope": "hour",
  "conc_ugm3": 28.5,
  "aqi": 85,
  "time": 1708345600
}
```

---

### Informacion del dispositivo
| Topic | `{BASE_TOPIC}/tele/INFO` |
|-------|------------------------|
| Cuándo | Al conectar al broker MQTT (retained) |

```json
{
  "fw": "0.0.0-dev",
  "build": "Feb 28 2026 15:30:00",
  "device": 2,
  "uptime_s": 3600,
  "ip": "192.168.1.100"
}
```

---

### Estado del sensor de particulas (requiere `ENABLE_PARTICLE_SENSOR`)
| Topic | `{BASE_TOPIC}/tele/PMS_STATE` |
|-------|------------------------------|
| Cuándo | Al cambiar de estado en el duty-cycle (retained) |

```json
{
  "state": "sleeping",
  "time": 1708345600
}
```

Estados posibles: `active`, `warmup`, `sleeping`.

---

## Topics suscritos (broker → dispositivo)

> Todos los comandos responden en `{BASE_TOPIC}/stat/{SUBSYSTEM}` con `{"status": "OK"|"ERR", "msg": "...", "time": ...}`.

### LED (requiere `ENABLE_NEOPIXEL`)
| Topic | `{BASE_TOPIC}/cmd/LED` |
|-------|----------------------|

```json
{
  "effect": "scanner",
  "dur_ms": 1200,
  "c1": "ff0000",
  "c2": "00ff00"
}
```

Efectos disponibles: `scanner`, `fade`.

---

### Buzzer (requiere `ENABLE_BUZZER`)
| Topic | Descripción |
|-------|-------------|
| `{BASE_TOPIC}/cmd/BUZZER` | Activar/desactivar: `{"enabled": true}` |
| `{BASE_TOPIC}/cmd/BUZZER/play` | Reproducir melodía RTTTL: `{"melody": "C5:2,G4:4"}` o parar: `{"stop": true}` |

---

### Calibración de sensores
| Topic | Descripción |
|-------|-------------|
| `{BASE_TOPIC}/cmd/OFFSETS` | Establecer offsets de calibración |
| `{BASE_TOPIC}/cmd/OFFSETS/get` | Leer offsets actuales |

```json
{
  "temp": 0.5,
  "hum": -2.0,
  "lux": 50.0,
  "press": -5.0
}
```

Todos los campos son opcionales. La respuesta incluye los offsets activos.

---

### Calibracion de microfono
| Topic | Descripcion |
|-------|-------------|
| `{BASE_TOPIC}/cmd/AUDIO_CAL` | Calibracion del microfono (noise floor) |
| `{BASE_TOPIC}/stat/AUDIO_CAL` | Respuesta de calibracion |

Acciones disponibles:
```json
{"action": "measure"}              // Mide noise floor (5s silencio)
{"action": "measure", "seconds": 10} // Medicion con duracion custom (2-30s)
{"action": "set", "noise_floor": -45.0} // Fijar valor manualmente
{"action": "get"}                  // Consultar valor actual
```

El resultado se almacena en EEPROM y se aplica automaticamente al reiniciar.
Ver [docs/MIC_CALIBRATION_GUIDE.md](docs/MIC_CALIBRATION_GUIDE.md) para detalles.

---

### Configuracion del sensor de particulas (requiere `ENABLE_PARTICLE_SENSOR`)
| Topic | Descripcion |
|-------|-------------|
| `{BASE_TOPIC}/cmd/PMS_CFG` | Configurar duty-cycle del sensor PMS |
| `{BASE_TOPIC}/stat/PMS_CFG` | Respuesta con configuracion actual |

Acciones disponibles:
```json
{"action": "get"}                          // Consultar config actual
{"action": "set", "sleep_min": 9, "wake_sec": 40}  // Activar duty-cycle
{"action": "set", "sleep_min": 0}          // Modo continuo (sin duty-cycle)
```

Respuesta ejemplo:
```json
{"status": "OK", "detail": "pms_cfg", "sleep_min": 9, "wake_sec": 40, "state": "active"}
```

- `sleep_min`: 0-60 (0 = continuo, sin duty-cycle)
- `wake_sec`: minimo `PMS_WARMUP_SEC + 10` (40s por defecto). Se ajusta automaticamente si el valor es menor.
- `state`: estado actual del PMS (`active`, `warmup`, `sleeping`)
- Los valores se persisten en EEPROM y se restauran al reiniciar.

---

### Sistema
| Topic | Descripcion |
|-------|-------------|
| `{BASE_TOPIC}/cmd/REBOOT` | Reiniciar el dispositivo |
| `{BASE_TOPIC}/stat/SYSTEM` | Respuesta de sistema |
| `{BASE_TOPIC}/cmd/HA` | Control de auto-discovery HA |
| `{BASE_TOPIC}/stat/HA` | Respuesta de HA discovery |

**Reboot**:
```json
{"now": true, "delay_ms": 500}
```

**Home Assistant Discovery**:
```json
{"action": "announce"}   // Publica configs de discovery para todas las entidades
{"action": "cleanup"}    // Borra todos los configs de discovery (retained vacios)
```

El flujo habitual para regenerar entidades en HA es: primero `cleanup`, luego `announce`.

---

### Topics globales

Los comandos enviados al topic global (`homebot/saqs/cmd/...`) aplican a todos los dispositivos simultaneamente (si `MQTT_LISTEN_TO_GLOBAL` esta activo en el dispositivo):

| Topic global | Equivalente por dispositivo |
|-------------|---------------------------|
| `homebot/saqs/cmd/BUZZER/play` | `{BASE_TOPIC}/cmd/BUZZER/play` |
| `homebot/saqs/cmd/LED` | `{BASE_TOPIC}/cmd/LED` |
| `homebot/saqs/cmd/REBOOT` | `{BASE_TOPIC}/cmd/REBOOT` |
| `homebot/saqs/cmd/HA` | `{BASE_TOPIC}/cmd/HA` |

---

## Home Assistant — Auto-discovery

Si `HA_ENABLE_DISCOVERY` está activo, al conectar se publican topics retenidos en `homeassistant/sensor/{CLIENT_ID}/{object_id}/config` para cada entidad:

| Object ID | Entidad | Campo JSON |
|-----------|---------|-----------|
| `temperature` | Temperatura | `temp_c` |
| `humidity` | Humedad | `hum_percent` |
| `pressure` | Presión | `press_mb` |
| `illuminance` | Luminosidad | `lux` |
| `co2` | CO2 | `co2_ppm` |
| `tvoc` | TVOC | `tvoc_ppm` |
| `pm1` | PM1.0 | `pm1_ugm3` |
| `pm25` | PM2.5 | `pm25_ugm3` |
| `pm10` | PM10 | `pm10_ugm3` |
| `hcho` | Formaldehído | `hcho` |
| `noise_dbfs` | Ruido (dBFS) | `dbfs` |
| `noise_ambient` | Ruido Ambiente (dBFS) | `dbfs_slow` |
| `aqi25` | AQI PM2.5 | — |
| `aqi10` | AQI PM10 | — |

La disponibilidad de cada entidad se vincula al topic LWT (`Online`/`Offline`).

---

# Debugging y logging

El logging es opcional y se activa por *flags* de compilación. La salida base (puerto y
baudrate) se define en [config/config.h](src/config/config.h) (`DEBUG_SERIAL_PORT`,
`DEBUG_BAUD`); los canales se habilitan por dispositivo en `config_device_X.h`:

| Flag | Canal |
|------|-------|
| `ENABLE_SERIAL_DEBUG` | Trazas de depuración por serie |
| `ENABLE_SERIAL_MQTT_DEBUG` | Tramas MQTT por serie |
| `ENABLE_MEGUNO_DEBUG` | Datos hacia MegunoLink |
| `ENABLE_MEGUNO_TIMEPLOT_DEBUG` | Series de sensores para TimePlot |

Si ninguno está activo, el código de logging no se compila (cero coste en flash/RAM).

Macros principales: `LOG_INFO` / `LOG_ERROR` / `DBG` para mensajes formateados,
`LOG_ONCE` y `LOG_EVERY_MS` para limitar la frecuencia, `SCOPE_TIMER` para medir el tiempo
de un bloque, y `DBG_HEXDUMP` para volcar buffers. Las macros `LOG_*` escriben a la vez por
serie y a MegunoLink cuando ambos canales están activos.

---

# Detector de palmas y silbidos

> **dBFS** = decibelios relativos a "full scale" del ADC (0 dBFS = saturación). Valores normales son negativos; cuanto más cerca de 0, más volumen.

Todos los parámetros se definen en `config.h` / `config_device_X.h`. A continuación, los más
relevantes para ajustar la detección.

### Muestreo / buffers

| Parámetro | Efecto |
|-----------|--------|
| `AUDIO_SAMPLE_BUFFER_SIZE` | Muestras por bloque. `block_ms ≈ size/rate·1000` (512 @ 16 kHz ≈ 32 ms). Más grande = más estable pero más latencia. |
| `AUDIO_SAMPLE_NUM_BUFFERS` | Nº de buffers. Más buffers = más margen contra dropouts a costa de RAM. |
| `AUDIO_SAMPLE_RATE_HZ` | Frecuencia de muestreo. Más alta = mejor respuesta a agudos y Goertzel, más CPU. |

### Palmas (FSM)

Envolvente (rectificación + IIR), umbral adaptativo `μ + K·σ` y FSM con validación temporal.

| Parámetro | Efecto (subir ⇒) |
|-----------|------------------|
| `AUDIO_CLAP_K_SIGMA` | Ganancia sobre σ en el umbral; subir = menos falsos, menos sensible. |
| `AUDIO_CLAP_SIGMA_FLOOR_DB` | Floor de σ en ambientes muy silenciosos (en cuentas de envolvente; el sufijo `_DB` es histórico). Subir = menos falsos en silencio. |
| `AUDIO_CLAP_TRIGGER_MIN_DB` | Gate absoluto en dBFS; más cerca de 0 = más estricto. |
| `AUDIO_CLAP_PEAK_MIN_MS` / `_MAX_MS` | Ventana válida de duración del pico (usa múltiplos de `block_ms`). |
| `AUDIO_CLAP_MIN_TIME_BETWEEN_MS` | Separación mínima entre palmas de una secuencia. |
| `AUDIO_CLAP_WAIT_AFTER_FIRST_MS` / `_PREV_MS` | Timeouts para cerrar la secuencia (1ª palma / palmas siguientes). |
| `AUDIO_CLAP_DEBOUNCE_MS` | Silencio tras publicar para evitar rebotes. |

### Silbidos (Goertzel, opcional)

Banco de *bins* tonales entre `FMIN`–`FMAX` con criterios de tonalidad, estabilidad y duración.

| Parámetro | Efecto |
|-----------|--------|
| `AUDIO_GOERTZEL_N` | Tamaño de bloque Goertzel; mayor = más resolución en frecuencia, más CPU. |
| `AUDIO_GOERTZEL_BINS` | Nº de frecuencias evaluadas entre `FMIN` y `FMAX`. |
| `AUDIO_GOERTZEL_FMIN_HZ` / `_FMAX_HZ` | Rango objetivo (silbido humano típico ~1.7–3.5 kHz). |
| `AUDIO_WHISTLE_TONALITY_MIN` | Tonalidad mínima = pico/total [0..1]; subir = exige tono más puro. |
| `AUDIO_WHISTLE_MIN_MS` | Duración acumulada mínima; subir = menos falsos. |
| `AUDIO_WHISTLE_DB_EXTRA_GATE` | dB extra sobre el gate de palmas. |
| `AUDIO_WHISTLE_DEBOUNCE_MS` | Silencio tras un silbido publicado. |
| `AUDIO_WHISTLE_GUARD_AFTER_CLAP_MS` | Mute de silbidos tras una palma válida (evita falsos). |
| `AUDIO_WHISTLE_FREQ_STABILITY_HZ` | Tolerancia para considerar el pico estable; subir = más permisivo con vibrato. |

### Consejos rápidos

- **Falsos claps**: sube `K_SIGMA` / `SIGMA_FLOOR_DB` y acerca `TRIGGER_MIN_DB` a 0.
- **No detecta palmas suaves**: baja `TRIGGER_MIN_DB` y `K_SIGMA`.
- **Falsos whistles** (golpes agudos): sube `TONALITY_MIN`, `DB_EXTRA_GATE` y `WHISTLE_MIN_MS`; mantén `GUARD_AFTER_CLAP_MS` activo.
- **No detecta silbidos reales**: baja `TONALITY_MIN` y `DB_EXTRA_GATE`, y ajusta `FMIN/FMAX` a tu tono.

---

## Licencia

[MIT](LICENSE) © 2026 mdps

---

> **Aviso:** proyecto experimental; las medidas son orientativas y no provienen de un instrumento calibrado/certificado. Entregado _tal cual_, sin garantía.

_Un proyecto de mdps · 2026 · desarrollado en Murcia._
