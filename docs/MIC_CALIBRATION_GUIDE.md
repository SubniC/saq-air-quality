# Guia de Calibracion de Microfonos - SAQAirQuality

## Por que es necesaria la calibracion

Cada dispositivo SAQ tiene un microfono electret diferente conectado al ADC del Photon.
Distintos microfonos (genericos chinos, Adafruit MAX4466, etc.) tienen sensibilidades
muy distintas, lo que afecta directamente al nivel dBFS medido en silencio.

| Parametro | Micro generico chino | Adafruit MAX4466 | Efecto |
|-----------|---------------------|------------------|--------|
| Sensibilidad tipica | -44 dBV/Pa | -44 dBV/Pa (ajustable via ganancia) | Mayor ganancia = mayor senal |
| Ruido propio | Alto (~-35 dBFS) | Bajo (~-50 dBFS) | Afecta al "suelo" de silencio |
| Bias/DC offset | Variable | Estable (Vcc/2) | Afecta al calculo de DC baseline |

### Impacto en la deteccion

El algoritmo de deteccion de palmas y silbidos usa dos mecanismos de umbral:

1. **Gate absoluto (`minDb`)**: Rechaza bloques por debajo de un nivel fijo (ej. -22 dBFS).
   Un micro poco sensible puede no alcanzar -22 dBFS ni con una palmada fuerte.

2. **Gate relativo (`relGateDb`)**: Requiere que la senal supere `ambient + relGateDb`.
   Si el ruido de fondo (noise floor) es muy diferente al esperado, este gate
   puede ser demasiado estricto o demasiado permisivo.

3. **Bootstrap del ambient**: Al arrancar, el detector necesita establecer una linea base.
   Sin calibracion, usa -90 dBFS y espera 3 bloques silenciosos. Con calibracion,
   arranca directamente con el valor medido.

## Procedimiento de calibracion

### Opcion A: Medicion automatica via MQTT

1. Asegurate de que el entorno esta en **silencio** (sin voces, musica, ventiladores fuertes).

2. Envia el comando MQTT de medicion:
   ```
   Topic:   homebot/SAQ-X/cmd/AUDIO_CAL
   Payload: {"action":"measure"}
   ```
   Por defecto mide durante 5 segundos. Para una medicion mas precisa:
   ```json
   {"action":"measure","seconds":10}
   ```

3. El dispositivo responde con el resultado:
   ```
   Topic:   homebot/SAQ-X/stat/AUDIO_CAL
   Payload: {"status":"OK","msg":"measured","noise_floor":-45.2}
   ```

4. El valor se guarda automaticamente en EEPROM y se aplica al detector.

5. **Verificacion**: consulta el valor almacenado:
   ```
   Topic:   homebot/SAQ-X/cmd/AUDIO_CAL
   Payload: {"action":"get"}
   ```

### Opcion B: Fijar valor manualmente

Si ya conoces el noise floor de un micro concreto:
```
Topic:   homebot/SAQ-X/cmd/AUDIO_CAL
Payload: {"action":"set","noise_floor":-45.0}
```

Rango valido: -90.0 a -5.0 dBFS.

### Opcion C: Leer logs por serie

Con `ENABLE_SERIAL_DEBUG` activo, observa los logs al arrancar:

```
{CLAP} ambient bootstrapped to -47.3 dBFS after 3 quiet blocks
```

Ese valor (-47.3) es el noise floor natural del micro. Puedes usarlo con la opcion B.

## Valores de referencia

| Dispositivo | Micro | Noise floor tipico | Notas |
|-------------|-------|-------------------|-------|
| SAQ-1 | Generico (protoboard, ya no existe) | ~-35 a -40 dBFS | Ruido alto, sin amplificador |
| SAQ-2 | Adafruit MAX4466 (despacho) | ~-48 a -55 dBFS | Ganancia ajustable via potenciometro |
| SAQ-3 | Generico (salon) | ~-38 a -45 dBFS | Depende del modelo concreto |
| SAQ-4 | Generico (dormitorio) | ~-38 a -45 dBFS | Depende del modelo concreto |

> **Nota**: Los valores anteriores son estimaciones. Cada unidad puede variar.
> Usa siempre la medicion automatica para obtener el valor real.

## Como afecta la calibracion al detector

Cuando se aplica un noise floor calibrado:

1. **Bootstrap inmediato**: El detector no necesita esperar 3 bloques silenciosos para
   establecer la linea base. Arranca directamente con el valor de EEPROM.

2. **Gate dinamico mas preciso**: `dynGate = max(minDb, ambient + relGateDb)`.
   Con un ambient mas preciso desde el inicio, el gate se ajusta mejor al micro real.

3. **Persistencia**: El valor sobrevive a reinicios. Se carga automaticamente al arrancar.

## Ejemplo de flujo completo con Home Assistant

Puedes crear una automatizacion en HA para calibrar al reiniciar:

```yaml
automation:
  - alias: "Calibrar micro SAQ-2 al reiniciar"
    trigger:
      - platform: mqtt
        topic: "homebot/SAQ-2/LWT"
        payload: "online"
    action:
      - delay: "00:00:10"  # esperar a que estabilice
      - service: mqtt.publish
        data:
          topic: "homebot/SAQ-2/cmd/AUDIO_CAL"
          payload: '{"action":"measure","seconds":10}'
```

## Endpoint MQTT: /cmd/AUDIO_CAL

| Accion | Payload | Respuesta |
|--------|---------|-----------|
| Medir noise floor | `{"action":"measure"}` o `{"action":"measure","seconds":N}` | `{"status":"OK","msg":"measuring","seconds":N}` seguido de `{"status":"OK","msg":"measured","noise_floor":-XX.X}` |
| Fijar manualmente | `{"action":"set","noise_floor":-45.0}` | `{"status":"OK","msg":"set","noise_floor":-45.0}` |
| Consultar actual | `{"action":"get"}` | `{"status":"OK","msg":"audio_cal","noise_floor":-45.0,"calibrated":"yes"}` |

## Troubleshooting

**El noise floor medido es muy alto (> -20 dBFS)**
- Hay ruido de fondo durante la medicion. Asegurate de silencio total.
- El micro puede estar danado o mal conectado.

**El noise floor medido es muy bajo (< -70 dBFS)**
- El micro tiene muy poca sensibilidad o la ganancia es muy baja.
- Verifica la conexion al pin ADC correcto (ver config_device_X.h).

**Las palmas no se detectan tras calibrar**
- Si el noise floor real es mucho mas alto que -22 dBFS (el `minDb` por defecto),
  las palmas puede que no superen el gate absoluto. Considera subir `minDb`
  en la configuracion del dispositivo o bajar la ganancia del micro.

**El valor no persiste tras reinicio**
- La EEPROM debe haber migrado al formato v2. Si el dispositivo tenia datos v1,
  la primera escritura fuerza un reset a defaults. Recalibra tras el primer reinicio.
