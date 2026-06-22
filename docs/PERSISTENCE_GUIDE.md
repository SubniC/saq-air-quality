# Persistence Module — Guia de referencia

El modulo `PERSISTENCE` gestiona la configuracion no volatil en la EEPROM emulada del Photon
(2048 bytes en flash) con integridad verificada (checksum FNV-1a), versionado de esquema y
proteccion contra desgaste.

## Layout y datos

La EEPROM empieza con un header de 12 bytes (`magic` 0x41514250 "AQBP", `version`, `length` =
`sizeof(Data)`, `checksum` FNV-1a del bloque Data) seguido del struct `Data` (~40 bytes):
offsets de sensores (`CalibOffsets`: temp/hum/lux/press), calibracion de microfono
(`AudioCalib`: mic_noise_floor), flags de neopixel/buzzer, brillo de LED, config de duty-cycle
del PMS (sleep_min/wake_sec), baseline del CCS811 y `reserved[9]` para futuras ampliaciones.
Total: ~52 de 2048 bytes.

El struct `Data` tiene layout fijo **sin `#ifdef`**: los campos de features deshabilitados se
ignoran en runtime pero mantienen el orden binario estable entre dispositivos.

## API publica

- `begin()` — inicializa (llamar una vez en `setup()`). Lee y valida la EEPROM; si falla
  (primera vez, version nueva, corrupcion) escribe defaults. Retorna `false` si la EEPROM es
  demasiado pequenia.
- `cfg()` -> `Data&` — acceso lectura/escritura a la copia en RAM.
- `mark_dirty()` — marca cambios pendientes; **siempre** tras modificar `cfg()`.
- `save()` — escribe RAM -> EEPROM con header+checksum y limpia el flag dirty.
- `save_if_dirty(min_interval_ms = 5000)` — guarda solo si hay cambios y paso el intervalo
  (con `0` fuerza guardado inmediato). El scheduler la llama cada 10 s como red de seguridad.
- `load()` — recarga desde EEPROM; si la validacion falla, restaura defaults.
- `is_valid()` -> `bool` — si la ultima carga fue valida.

Patron tipico en un handler MQTT:
```cpp
PERSISTENCE::cfg().offsets.temp = new_value;
PERSISTENCE::mark_dirty();
PERSISTENCE::save_if_dirty(0);  // inmediato
```
Para cambios no criticos, omitir el `save_if_dirty(0)` y dejar que el scheduler guarde.

## Integridad y validacion

El checksum FNV-1a se calcula sobre todo el bloque Data (rapido, no criptografico, suficiente
para detectar corrupcion). En la carga se verifican 4 condiciones: `magic`, `version`,
`length == sizeof(Data)` y `checksum`. Si cualquiera falla, se restauran defaults y se
sobreescribe la EEPROM.

## Versionado

Esquema actual: v5. Historico: v1-v2 (offsets + mic_noise_floor), v3 (AudioCalib separado,
layout fijo), v4 (duty-cycle PMS), v5 (baseline CCS811). Al subir de version, los dispositivos
antiguos hacen auto-reset a defaults en el primer boot (la validacion falla por version
mismatch); es intencional, ya que el layout binario ha cambiado.

## Proteccion contra desgaste

`save_if_dirty()` con intervalo minimo evita escrituras excesivas; la tarea del scheduler solo
escribe cada 10 s si hay cambios. Los handlers MQTT usan guardado inmediato (eventos
esporadicos). La EEPROM del Photon es emulada en flash con wear-leveling interno.

## Anadir un campo nuevo

1. Anadir el campo al struct `Data` en `persistence.h` (antes de `reserved[]`).
2. Reducir `reserved[]` en los bytes anadidos.
3. Incrementar `VERSION` en `persistence.cpp`.
4. Inicializarlo en `to_defaults()`.
5. Verificar con `static_assert` que cabe en EEPROM.

Nota: los dispositivos con la version anterior perderan su config al actualizar (auto-reset),
salvo que se anada logica de migracion explicita.

## Ficheros

- `src/persistence.h` — structs y API publica.
- `src/persistence.cpp` — implementacion (header, checksum, EEPROM I/O).
