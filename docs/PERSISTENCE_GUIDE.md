# Persistence Module — Guia de referencia

## Resumen

El modulo de persistencia (`PERSISTENCE`) gestiona la configuracion no volatil del dispositivo en la EEPROM emulada del Particle Photon (2048 bytes en flash). Proporciona almacenamiento con integridad verificada (checksum FNV-1a), versionado de esquema, y proteccion contra desgaste de flash.

## Arquitectura

### Layout en EEPROM

```
Direccion 0x0000:
+------------------+
|     Header       |  12 bytes (packed)
|  magic    (u32)  |  0x41514250 ("AQBP")
|  version  (u16)  |  Version del esquema (actualmente v5)
|  length   (u16)  |  sizeof(Data) — para detectar cambios de tamanio
|  checksum (u32)  |  FNV-1a de todo el bloque Data
+------------------+
|     Data         |  ~40 bytes (ver detalle abajo)
|  CalibOffsets    |  16 bytes (4 floats)
|  AudioCalib     |  4 bytes (1 float)
|  neopixel_en    |  1 byte
|  led_brightness |  1 byte
|  buzzer_enabled |  1 byte
|  pms_sleep_min  |  1 byte
|  pms_wake_sec   |  1 byte
|  ccs811_baseline|  2 bytes (uint16_t, registro 0x11 del CCS811)
|  reserved[9]    |  9 bytes (para futuras ampliaciones)
+------------------+
Total: ~52 bytes de 2048 disponibles
```

### Struct Data (v5)

```cpp
struct CalibOffsets {
    float temp;     // Offset aditivo temperatura (C)
    float hum;      // Offset aditivo humedad (%RH)
    float lux;      // Offset aditivo lux
    float press;    // Offset aditivo presion (mBar)
};

struct AudioCalib {
    float mic_noise_floor;  // dBFS de silencio medido (0 = sin calibrar)
};

struct Data {
    CalibOffsets offsets;      // Correcciones de sensores analogicos
    AudioCalib   audio;       // Calibracion de microfono
    uint8_t neopixel_enabled; // 1=LEDs activos, 0=apagados
    uint8_t led_brightness;   // Brillo LEDs (0-255)
    uint8_t buzzer_enabled;   // 1=buzzer activo, 0=silenciado
    uint8_t pms_sleep_min;    // Minutos de sleep en duty-cycle (0=continuo)
    uint8_t pms_wake_sec;     // Segundos de wake (incluye warmup)
    uint16_t ccs811_baseline; // Baseline register CCS811 (0 = sin calibrar)
    uint8_t reserved[9];      // Reserva para futuro (inicializado a 0)
};
```

**Principio de disenio**: El struct Data tiene layout fijo sin `#ifdef`. Los campos de features deshabilitados se ignoran en runtime pero mantienen el orden binario estable entre dispositivos con distinta configuracion.

## API publica

### `PERSISTENCE::begin()` -> `bool`

Inicializa el modulo. Llamar una vez en `setup()`.

- Lee la EEPROM y valida: magic, version, length, checksum
- Si es valido: carga los datos en RAM
- Si no es valido (primera vez, version nueva, corrupcion): escribe defaults y guarda
- Retorna `false` si la EEPROM es demasiado pequenia para el layout

### `PERSISTENCE::cfg()` -> `Data&`

Acceso directo de lectura/escritura a la copia en RAM. Alias de `data()`.

```cpp
// Leer un valor
float nf = PERSISTENCE::cfg().audio.mic_noise_floor;

// Modificar un valor
PERSISTENCE::cfg().offsets.temp = -1.5f;
PERSISTENCE::mark_dirty();
```

### `PERSISTENCE::mark_dirty()`

Marca que hay cambios pendientes en RAM que no se han guardado en EEPROM. **Siempre** llamar despues de modificar `cfg()`.

### `PERSISTENCE::save()`

Escribe inmediatamente la copia RAM a EEPROM con header+checksum. Resetea el flag dirty.

### `PERSISTENCE::save_if_dirty(min_interval_ms = 5000)`

Guarda solo si hay cambios pendientes Y ha pasado al menos `min_interval_ms` desde el ultimo guardado. Evita desgaste excesivo de la flash.

- Con `min_interval_ms = 0`: fuerza guardado inmediato (util para MQTT handlers)
- El scheduler llama periodicamente `save_if_dirty()` cada 10 segundos como red de seguridad

### `PERSISTENCE::load()`

Fuerza recarga desde EEPROM. Si la validacion falla, restaura defaults.

### `PERSISTENCE::is_valid()` -> `bool`

Indica si la ultima carga fue valida.

## Patron de uso tipico

### En setup()

```cpp
if (PERSISTENCE::begin()) {
    LOG_INFO("Persistence loaded");
}
```

### Leer configuracion

```cpp
float nf = PERSISTENCE::cfg().audio.mic_noise_floor;
uint8_t brightness = PERSISTENCE::cfg().led_brightness;
```

### Modificar y guardar (handler MQTT)

```cpp
PERSISTENCE::cfg().offsets.temp = new_value;
PERSISTENCE::mark_dirty();
PERSISTENCE::save_if_dirty(0);  // guardar inmediatamente
```

### Modificar con guardado diferido

```cpp
PERSISTENCE::cfg().led_brightness = new_brightness;
PERSISTENCE::mark_dirty();
// El scheduler guardara en su proxima pasada (cada 10s)
```

## Versionado

| Version | Cambios |
|---------|---------|
| v1 | Layout inicial: CalibOffsets con mic_noise_floor incluido |
| v2 | +mic_noise_floor en CalibOffsets |
| v3 | AudioCalib separado de CalibOffsets, layout fijo sin #ifdef |
| v4 | +pms_sleep_min, pms_wake_sec para duty-cycling del PMS |
| v5 | +ccs811_baseline (uint16_t) para save/restore del baseline CCS811 |

**Cuando se cambia la version**: Los dispositivos con version anterior hacen auto-reset a defaults en el primer boot (la validacion falla por version mismatch). Esto es intencional — el layout binario ha cambiado y los datos antiguos serian incoherentes.

## Integridad

### Checksum FNV-1a

Se calcula sobre todo el bloque `Data` (no incluye el header). FNV-1a es rapido, tiene buena distribucion, y es adecuado para deteccion de corrupcion (no es criptografico).

```
hash = 2166136261
for each byte:
    hash ^= byte
    hash *= 16777619
```

### Validacion en carga

Se verifican 4 condiciones:
1. `magic == 0x41514250` — identifica que la EEPROM tiene datos nuestros
2. `version == VERSION` — esquema compatible
3. `length == sizeof(Data)` — tamanio esperado
4. `checksum == fnv1a32(data)` — integridad de datos

Si cualquiera falla, se restauran defaults y se sobreescribe la EEPROM.

## Proteccion contra desgaste

- `save_if_dirty()` con intervalo minimo evita escrituras excesivas
- Tarea periodica en scheduler: cada 10 segundos, solo escribe si hay cambios pendientes
- Los handlers MQTT usan `save_if_dirty(0)` para guardado inmediato (aceptable porque son eventos esporadicos)
- La EEPROM del Photon es emulada en flash con wear-leveling interno

## Valores por defecto

| Campo | Default | Notas |
|-------|---------|-------|
| offsets.temp | 0.0 | Sin correccion |
| offsets.hum | 0.0 | Sin correccion |
| offsets.lux | 0.0 | Sin correccion |
| offsets.press | 0.0 | Sin correccion |
| audio.mic_noise_floor | 0.0 | Sin calibrar (valor valido < -10 dBFS) |
| neopixel_enabled | 1 | LEDs activos |
| led_brightness | 50 | Brillo moderado |
| buzzer_enabled | 1 | Buzzer activo |
| pms_sleep_min | 0 | 0 = continuo (activar duty-cycle via MQTT) |
| pms_wake_sec | 40 | 40 segundos despierto (cuando duty-cycle activo) |
| ccs811_baseline | 0 | Sin calibrar (valor valido != 0, restaurado tras 20min warmup) |

## Endpoints MQTT relacionados

| Endpoint | Campos que modifica |
|----------|-------------------|
| `/cmd/OFFSETS` | offsets.temp, .hum, .lux, .press |
| `/cmd/OFFSETS/get` | (lectura) |
| `/cmd/AUDIO_CAL` | audio.mic_noise_floor |
| `/cmd/PMS_CFG` | pms_sleep_min, pms_wake_sec |
| `/cmd/LED` | neopixel_enabled, led_brightness |
| `/cmd/BUZZER` | buzzer_enabled |

## Detalle endpoint PMS_CFG

Topic: `{MQTT_BASE_TOPIC}/cmd/PMS_CFG`
Respuesta: `{MQTT_BASE_TOPIC}/stat/PMS_CFG`

### Obtener configuracion actual

```json
{"action":"get"}
```

Respuesta:
```json
{"status":"OK","detail":"pms_cfg","sleep_min":0,"wake_sec":40,"state":"active"}
```

El campo `state` puede ser: `active`, `warmup`, `sleeping`.

### Activar duty-cycling

```json
{"action":"set","sleep_min":9,"wake_sec":40}
```

El sensor dormira 9 minutos, despertara 40 segundos (30s warmup + 10s lecturas utiles), y volvera a dormir. Esto extiende la vida del ventilador/laser de ~1 anio a 3+ anios.

### Desactivar duty-cycling (modo continuo)

```json
{"action":"set","sleep_min":0}
```

Con `sleep_min=0` el sensor lee continuamente (~1Hz). Es el default de fabrica.

### Validacion

- `sleep_min`: 0-60 (0 = continuo)
- `wake_sec`: automaticamente ajustado a minimo `PMS_WARMUP_SEC + 10` (40s) si el valor es menor

## Como aniadir un nuevo campo

1. Aniadir el campo al struct `Data` en `persistence.h` (antes de `reserved[]`)
2. Reducir `reserved[]` en la misma cantidad de bytes aniadidos
3. Incrementar `VERSION` en `persistence.cpp`
4. Aniadir inicializacion en `to_defaults()`
5. Compilar y verificar con `static_assert` que cabe en EEPROM

**Nota**: Los dispositivos con la version anterior perderan su configuracion al actualizar (auto-reset a defaults). Si se necesita migracion, habria que aniadir logica de lectura de la version anterior y mapeo de campos.

## Ficheros

- `src/persistence.h` — Structs y API publica
- `src/persistence.cpp` — Implementacion (header, checksum, EEPROM I/O)
