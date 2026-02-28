# Analisis del sistema de deteccion de palmas y silbidos

Fecha: Febrero 2026
Dispositivo objetivo: Particle Photon (ARM Cortex-M3 @ 72 MHz, sin FPU, 128 KB RAM)

---

## 1. Arquitectura actual

### Pipeline de audio

```
ISR (62.5 us)          Main loop (scheduler)
     |                       |
 analogRead(MIC)        task_sound_loop (10 ms period)
     |                       |
 buffer[i] = sample      SOUND::loop()
     |                       |
 index++                 if (sendIndex < sampleIndex)
     |                       |
 if full:                parseBucket() -> dBFS + feedBlock()
   free=false               |
   sampleIndex++         processBucket() -> noise averages
                             |
                         sendIndex++, free=true
```

- **Muestreo**: 16 kHz via IntervalTimer (ISR cada 62.5 us)
- **Buffer**: 512 muestras = 32 ms por bloque
- **Doble buffering**: ISR rota entre buffers; main procesa el lleno

### Deteccion de palmas (FSM)

```
IDLE --(hit)--> RISING --(no hit)--> PEAK --> FALLING --(no hit)--> GAP --> IDLE
                                       |                             |
                                  valida duracion              espera timeout
                                  + nivel minimo               notifica callback
```

- Umbral adaptativo: `mu + K * sigma` (envolvente IIR)
- Gate dinamico: `max(minDb, ambientDb + relGateDb)`
- Estadisticas (mu, sigma) solo se actualizan en IDLE/GAP sin hit

### Deteccion de silbidos (Goertzel)

- 12 bins de frecuencia entre 2000-5500 Hz
- Ventana de 256 muestras (~16 ms)
- Criterios: tonalidad >= umbral AND frecuencia estable (+- tol) AND duracion >= minMs
- Guards: post-clap mute, debounce, extra dB gate

---

## 2. Analisis de timing del scheduler

### Tabla de tareas (caso completo: LCD + buzzer + neopixel)

| Tarea | Periodo | Estimacion duracion | Criticidad |
|-------|---------|-------------------|------------|
| wdt | 1250 ms | <0.1 ms | baja |
| sound_loop | 10 ms | 5-8 ms | CRITICA |
| buzzer_loop | 5 ms | <0.5 ms | baja |
| leds_loop | 5 ms | <0.5 ms | baja |
| mqtt_loop | 50 ms | 5-10 ms | media |
| screen | 50 ms | 0.5-25 ms (*) | ALTA |
| temperature | 4500 ms | 2-5 ms (I2C) | baja |
| lux | 5000 ms | 1-2 ms (I2C) | baja |
| gas | 1 ms | <1 ms | baja |
| particles | 1 ms | <1 ms (UART poll) | baja |
| mqtt_sensors | 90000 ms | 3-5 ms | baja |
| mqtt_noise | 90000 ms | 1-2 ms | baja |
| health | 1000 ms | <1 ms | baja |

(*) `task_screen` incluye I2C flush al OLED: 1024 bytes @ 400 kHz = ~25 ms en peor caso.

### Peor caso por pasada del scheduler

El scheduler ejecuta TODAS las tareas pendientes en una pasada (con budget_ms=100):

```
Caso tipico:    sound(6ms) + mqtt(5ms) + screen_paint(1ms) = 12 ms   -> OK
Peor caso:      sound(8ms) + mqtt(10ms) + screen_flush(25ms) = 43 ms -> RIESGO con 2 buffers
Peor absoluto:  Todo lo anterior + temperature_i2c(5ms) = 48 ms      -> RIESGO con 2 buffers
```

### Tolerancia de buffers

| Num buffers | Tolerancia max (ms) | Margen sobre peor caso |
|-------------|--------------------|-----------------------|
| 2 (actual) | 1 x 32 = 32 ms | 32 - 43 = **-11 ms (INSUFICIENTE)** |
| 3 | 2 x 32 = 64 ms | 64 - 43 = 21 ms |
| 4 (propuesto) | 3 x 32 = 96 ms | 96 - 43 = **53 ms (SEGURO)** |

### Coste de RAM

| Num buffers | RAM por buffer | Total | Delta |
|-------------|---------------|-------|-------|
| 2 | 1028 bytes | 2056 bytes | - |
| 4 | 1028 bytes | 4112 bytes | +2056 bytes |

Photon tiene 128 KB de RAM. El incremento de 2 KB es asumible (~1.6%).

### Conclusion de timing

Con 2 buffers, el sistema puede perder muestras cuando `task_screen` hace flush I2C
al OLED coincidiendo con `task_mqtt_loop`. Con 4 buffers el margen es de 53 ms,
suficiente para absorber cualquier combinacion de tareas.

La tarea `task_sound_loop` con periodo=10 ms y catchup=true asegura que los buffers
se procesan a tiempo cuando el scheduler no esta sobrecargado.

---

## 3. Coste computacional de la deteccion

Estimaciones para Cortex-M3 @ 72 MHz sin FPU (float por software ~20 ciclos/op):

| Operacion | Iteraciones | Estimacion |
|-----------|-------------|-----------|
| `_update_baseline_IIR` | 512 x 3 float | ~0.4 ms |
| `_calculate_RMS` | 512 x 4 float + sqrt | ~0.6 ms |
| `updateEnvelope_` | 512 x 3 float | ~0.4 ms |
| `goertzelPowerSum_` | 12 bins x 256 int32 | ~0.5 ms |
| Stats + FSM | O(1) | <0.1 ms |
| **Total por bloque** | | **~2 ms** |

Con `task_sound_loop` a 10 ms de periodo, se procesan ~3 bloques por segundo mas de
lo minimo necesario. El headroom es adecuado.

---

## 4. Bugs identificados y correcciones

### BUG-1: Perdida silenciosa de muestras en ISR (CRITICO)

**Problema**: Cuando el ISR encuentra el buffer lleno (`!sb->free`), simplemente retorna
sin contabilizar la perdida. No hay forma de saber si se estan perdiendo datos.

**Impacto**: Palmas que coinciden con picos de carga del scheduler desaparecen sin aviso.

**Correccion**: Anadir `volatile uint32_t g_audio_dropped_blocks` incrementado en el ISR.
Loguear periodicamente en `SOUND::loop()`.

### BUG-2: Bootstrap de ambient contaminado (MEDIO)

**Problema**: En `feedBlock()`, la primera vez que `ambientDb_ < -80`:
```cpp
ambientDb_ = block_db;  // acepta cualquier valor, incluso una palma
```

Si el dispositivo arranca durante un ruido fuerte, el ambient queda alto (~10s para corregirse)
y el gate dinamico rechaza palmas reales.

**Correccion**: Requerir N bloques consecutivos con `block_db < -35 dBFS` antes de aceptar
el valor como baseline.

### BUG-3: Whistle stability check falla los primeros 2 bloques (MENOR)

**Problema**: `freqStable_()` necesita 2 valores historicos. Las posiciones del historial
se inicializan a 0, y el check `if (!a || !b) return false` rechaza los primeros 2 bloques.

**Impacto**: Los primeros ~64 ms de un silbido no cuentan para `whistleMsAcc_`.
Con `whistleMinMs=400`, el silbido real necesita durar ~464 ms.

**Correccion**: Pre-seed del historial de frecuencias en el primer bloque con tonalidad valida.

### BUG-4: No hay limite maximo de gap entre claps (MENOR)

**Problema**: Solo existe `MIN_TIME_BETWEEN_MS` (140 ms). Dos golpes accidentales separados
por 3 segundos se cuentan como secuencia de 2 palmas.

**Correccion**: Anadir `AUDIO_CLAP_MAX_TIME_BETWEEN_MS` (~1200 ms). Si el gap excede
este valor, resetear la secuencia.

### BUG-5: Umbral sin histeresis (MENOR)

**Problema**: El mismo umbral `mu + K*sigma` se usa para entrar (IDLE->RISING) y para
salir (RISING->PEAK). Senales cerca del umbral causan oscilaciones rapidas entre estados.

**Correccion**: Usar Schmitt trigger: umbral de entrada `kSigma` y umbral de mantenimiento
`kSigmaFalling` (menor), evitando re-entradas espurias.

---

## 5. Ajustes de parametros

### Valores especificos SAQ-2 (config_device_2.h — Adafruit MAX4466)

SAQ-2 usa un Adafruit MAX4466 (op-amp integrado, 20x-100x ganancia hardware),
significativamente mas sensible que los micros electret genericos de SAQ-1/SAQ-3.

**Problema diagnosticado (Feb 2026)**: Falsos positivos frecuentes. Golpes en la mesa
a -18.8 dBFS con ambiente a -32.5 dBFS (SNR +13.7 dB) disparaban detecciones.

**Datos de campo**:
- Ambiente tipico despacho: -42 a -45 dBFS
- Palma real a 1-2m: -4 a -8 dBFS (SNR +34 a +41 dB)
- Golpe mesa: -18 dBFS (SNR ~+14 dB)

| Parametro | Generico | SAQ-2 | Razon |
|-----------|----------|-------|-------|
| `REL_GATE_DB` | 8.0 | **15.0** | Exigir +15 dB SNR; palma real da +34..+41 |
| `K_SIGMA` | 2.5 | **3.0** | Micro sensible, envolvente mas variable |
| `K_SIGMA_FALLING` | 2.0 | **2.2** | Schmitt trigger mas estricto |
| `TRIGGER_MIN_DB` | -22.0 | **-15.0** | MAX4466 capta golpes domesticos a -18 |
| `DEBOUNCE_MS` | 2000 | **500** | Respuesta mas agil tras deteccion |
| `WAIT_AFTER_FIRST_MS` | 1000 | **800** | Single clap mas rapido |
| `WAIT_AFTER_PREV_MS` | 1500 | **900** | Secuencias mas rapidas |

### Valores actuales vs propuestos (config_device_1.h / config_device_3.h)

| Parametro | Actual | Propuesto | Razon |
|-----------|--------|-----------|-------|
| `AUDIO_SAMPLE_NUM_BUFFERS` | 2 | 4 | Eliminar drops por jitter del scheduler |
| `AUDIO_CLAP_K_SIGMA` | 3.2 | 2.5 | Mas sensible manteniendo selectividad |
| `AUDIO_CLAP_K_SIGMA_FALLING` | (no existia) | 2.0 | Histeresis para evitar oscilaciones |
| `AUDIO_CLAP_TRIGGER_MIN_DB` | -26.0 | -22.0 | Filtrar ruido domestico (AC, puertas) |
| `AUDIO_CLAP_PEAK_MIN_MS` | 30 | 25 | Palmas rapidas en sala reverberante |
| `AUDIO_CLAP_PEAK_MAX_MS` | 200 | 250 | Mas margen para reverberacion |
| `AUDIO_CLAP_MAX_TIME_BETWEEN_MS` | (no existia) | 1200 | Evitar secuencias falsas |
| `AUDIO_WHISTLE_TONALITY_MIN` | 0.85 | 0.75 | Silbidos humanos no son tonos puros |
| `AUDIO_WHISTLE_MIN_MS` | 400 | 300 | Respuesta mas rapida |

### Logica de los cambios

**K_SIGMA 3.2 -> 2.5**: Con K=3.2, el umbral de envolvente es mu + 3.2*sigma.
Esto es muy estricto: solo pasa un evento por encima de 3.2 desviaciones estandar.
Para un sensor de habitacion con micro electret, 2.5 sigma da mejor balance
sensibilidad/especificidad.

**K_SIGMA_FALLING 2.0 (nuevo)**: Crea una banda de histeresis de 0.5*sigma
entre el umbral de entrada (2.5*sigma) y el de mantenimiento (2.0*sigma).
Evita que senales fluctuantes en torno al umbral generen transiciones IDLE->RISING->PEAK
multiples en un mismo clap.

**TRIGGER_MIN_DB -26 -> -22**: En un entorno domestico, -26 dBFS incluye
portazos lejanos, agua del grifo, etc. -22 dBFS requiere un sonido claramente
audible (una palma real a 2-3 metros).

**PEAK_MAX_MS 200 -> 250**: Una palma en una sala con paredes duras puede tener
una cola de reverberacion de ~200 ms. Con el limite justo a 200 ms, palmas reales
se rechazan por "demasiado largas".

**WHISTLE_TONALITY_MIN 0.85 -> 0.75**: Un silbido humano real tiene armonicos
y vibrato. Tonalidad de 0.85 exige un tono casi puro (oscilador). 0.75 acepta
silbidos reales manteniendo buena discriminacion contra ruidos de banda ancha.

---

## 6. Resumen de cambios implementados

### Ficheros modificados

| Fichero | Cambio |
|---------|--------|
| `config/config.h` | `AUDIO_SAMPLE_NUM_BUFFERS` 2 -> 4 |
| `sound.h` | Declaracion de `g_audio_dropped_blocks` |
| `sound.cpp` | Contador de drops en ISR + log periodico |
| `libs/ClapDetector.h` | Nuevos campos: `maxGapMs`, `kSigmaFalling`, `ambientBootCount_` |
| `libs/ClapDetector.cpp` | Bootstrap protegido, maxGap, histeresis, whistle warmup |
| `libs/SoundMeter.cpp` | Paso de nuevos parametros de config a ClapConfig |
| `config/config_device_1.h` | Parametros ajustados + nuevos defines |
| `config/config_device_3.h` | Parametros ajustados + nuevos defines |

### Compatibilidad

- **Audio basico (medida de ruido)**: Sin cambios en SoundMeter ni en el pipeline de noise averages.
- **4 buffers**: El ISR, `SOUND::loop()` y `start_sampling()` usan `AUDIO_SAMPLE_BUFFERS`
  como constante. El cambio de 2 a 4 es transparente: el indexado `% AUDIO_SAMPLE_BUFFERS`
  y el bucle de inicializacion en `start_sampling()` funcionan con cualquier N.
- **Dispositivos sin clap/whistle**: Los cambios en ClapDetector solo se compilan cuando
  `AUDIO_ENABLE_DETECT_CLAPS` o `AUDIO_ENABLE_WHISTLE_GOERTZEL` estan definidos.
  Los dispositivos con estas features deshabilitadas no se ven afectados.
- **Nuevos campos en ClapConfig**: Tienen valores por defecto en el struct
  (`maxGapMs=0`, `kSigmaFalling=0.0f`), por lo que codigo existente que no los
  establece funciona sin cambios (comportamiento identico al anterior).
