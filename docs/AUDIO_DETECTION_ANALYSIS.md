# Analisis del sistema de deteccion de palmas y silbidos

Dispositivo objetivo: Particle Photon (ARM Cortex-M3 @ 72 MHz, sin FPU, 128 KB RAM).

## Pipeline de audio

Un ISR a 16 kHz (cada 62.5 us) lee el ADC del microfono y rellena buffers de 512 muestras
(~32 ms/bloque). El ISR solo produce; el consumidor en el main loop (`SOUND::loop`, tarea de
10 ms) procesa el bloque completo: calcula dBFS, alimenta el detector y actualiza las medias
de ruido.

- **Palmas (FSM)**: envolvente IIR + umbral adaptativo `mu + K*sigma` con gate dinamico
  `max(minDb, ambientDb + relGateDb)`. Las estadisticas (mu, sigma) solo se actualizan en
  IDLE/GAP sin hit. FSM: IDLE -> RISING -> PEAK -> FALLING -> GAP, validando duracion de pico
  y nivel minimo.
- **Silbidos (Goertzel)**: 12 bins entre 2000-5500 Hz, ventana de 256 muestras. Exige
  tonalidad, frecuencia estable y duracion minima. Guards: mute post-clap, debounce y gate
  extra de dB.

## Timing del scheduler

`task_sound_loop` es la tarea critica (periodo 10 ms, `catchup=true`). El peor caso por
pasada combina audio + MQTT + flush I2C del OLED (este ultimo ~25 ms para 1024 B @ 400 kHz),
alcanzando ~43 ms.

Tolerancia segun nº de buffers (`(N-1) * 32 ms`):

| Buffers | Tolerancia | Margen sobre peor caso (~43 ms) |
|---------|-----------|---------------------------------|
| 2 | 32 ms | -11 ms (insuficiente) |
| 4 | 96 ms | +53 ms (seguro) |

El coste de pasar de 2 a 4 buffers es ~2 KB de RAM (~1.6% de los 128 KB), asumible. El coste
computacional de la deteccion por bloque es ~2 ms, con headroom amplio frente al periodo.

## Bugs corregidos

1. **Perdida silenciosa de muestras en el ISR**: cuando el buffer estaba lleno se descartaban
   datos sin contabilizar. Corregido con un contador `g_audio_dropped_blocks` (volatile)
   logueado periodicamente.
2. **Bootstrap de ambient contaminado**: al arrancar con ruido fuerte, el ambient quedaba
   alto y rechazaba palmas reales. Ahora se exigen N bloques consecutivos por debajo de
   -35 dBFS antes de aceptar el baseline.
3. **Whistle warmup**: el check de estabilidad de frecuencia rechazaba los primeros 2 bloques.
   Corregido con pre-seed del historial en el primer bloque tonal valido.
4. **Sin gap maximo entre claps**: dos golpes separados varios segundos se contaban como
   secuencia. Anadido `AUDIO_CLAP_MAX_TIME_BETWEEN_MS` (~1200 ms) para resetear la secuencia.
5. **Umbral sin histeresis**: el mismo umbral servia para entrar y salir, causando
   oscilaciones. Anadido Schmitt trigger con `kSigmaFalling` (umbral de mantenimiento menor).

## Ajuste de parametros

Los dispositivos con microfono mas sensible (Adafruit MAX4466, op-amp integrado) necesitan
gates mas estrictos que los electret genericos, para no disparar con golpes domesticos. Por
ejemplo, SAQ-2 (MAX4466) usa `REL_GATE_DB=15`, `K_SIGMA=3.0` y `TRIGGER_MIN_DB=-15`, frente a
valores mas sensibles en los dispositivos con micro generico.

Criterios generales:
- `K_SIGMA` mas bajo = mas sensible; valores ~2.5 dan buen balance con micro electret.
- `TRIGGER_MIN_DB` mas cerca de 0 filtra ruido domestico (portazos, agua, AC).
- `PEAK_MAX_MS` debe dar margen a la cola de reverberacion (~250 ms en salas duras).
- `WHISTLE_TONALITY_MIN` ~0.75 acepta silbidos humanos reales (con armonicos/vibrato)
  manteniendo discriminacion frente a ruido de banda ancha.

## Compatibilidad

- La medida de ruido no cambia; el pipeline de noise averages es independiente.
- El paso de 2 a 4 buffers es transparente (indexado `% N` y bucle de init parametrizados).
- Los cambios del detector solo se compilan con `AUDIO_ENABLE_DETECT_CLAPS` /
  `AUDIO_ENABLE_WHISTLE_GOERTZEL`. Los campos nuevos de `ClapConfig` tienen defaults neutros,
  de modo que el codigo existente mantiene el comportamiento previo.
