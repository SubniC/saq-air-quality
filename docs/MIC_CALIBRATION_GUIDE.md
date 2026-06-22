# Guia de Calibracion de Microfonos

## Por que es necesaria

Cada dispositivo SAQ usa un microfono distinto (electret generico, Adafruit MAX4466, etc.)
con sensibilidades y ruido propio muy diferentes, lo que cambia el nivel dBFS medido en
silencio. La calibracion fija ese "noise floor" para que la deteccion de palmas/silbidos
funcione igual en todos los dispositivos.

El detector usa un gate absoluto (`minDb`), un gate relativo (`ambient + relGateDb`) y un
bootstrap del ambient al arrancar. Con un noise floor calibrado, el detector arranca
directamente con el valor correcto (sin esperar bloques silenciosos) y el gate dinamico se
ajusta mejor al micro real.

## Procedimiento (MQTT)

Medicion automatica (en silencio):
```
Topic:   homebot/SAQ-X/cmd/AUDIO_CAL
Payload: {"action":"measure"}              # 5s por defecto
         {"action":"measure","seconds":10} # duracion custom (2-30s)
```
El dispositivo responde en `.../stat/AUDIO_CAL` con `{"noise_floor":-45.2}` y guarda el valor
en EEPROM (se aplica al arrancar).

Fijar manualmente (rango -90.0 a -5.0 dBFS):
```
Payload: {"action":"set","noise_floor":-45.0}
```

Consultar valor actual: `{"action":"get"}`.

Tambien se puede leer el noise floor natural en los logs serie (con `ENABLE_SERIAL_DEBUG`):
`{CLAP} ambient bootstrapped to -47.3 dBFS ...`.

## Valores de referencia

| Micro | Noise floor tipico |
|-------|-------------------|
| Generico (sin amplificador) | ~-35 a -45 dBFS |
| Adafruit MAX4466 | ~-48 a -55 dBFS (ganancia ajustable) |

Son estimaciones; cada unidad varia. Usa siempre la medicion automatica para el valor real.

## Endpoint /cmd/AUDIO_CAL

| Accion | Payload | Respuesta |
|--------|---------|-----------|
| Medir | `{"action":"measure"[,"seconds":N]}` | `measuring` y luego `measured` + `noise_floor` |
| Fijar | `{"action":"set","noise_floor":-45.0}` | `set` + `noise_floor` |
| Consultar | `{"action":"get"}` | `noise_floor` + `calibrated` |

## Troubleshooting

- **Noise floor > -20 dBFS**: ruido de fondo durante la medicion, o micro danado/mal conectado.
- **Noise floor < -70 dBFS**: micro poco sensible o ganancia muy baja; verifica el pin ADC.
- **No detecta palmas tras calibrar**: si el noise floor real supera el `minDb` por defecto
  (-22 dBFS), sube `minDb` o baja la ganancia del micro.
- **El valor no persiste**: tras migrar de un formato EEPROM antiguo, la primera escritura
  resetea a defaults; recalibra tras el primer reinicio.

Ver [PERSISTENCE_GUIDE.md](PERSISTENCE_GUIDE.md) para el almacenamiento en EEPROM.
