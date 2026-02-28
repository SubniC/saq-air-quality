#pragma once

#include "config/config_device.h"

#define DEVICE_SOFTWARE_VERSION "0.0.0-dev"
#define DEVICE_BUILD_TIMESTAMP  __DATE__ " " __TIME__

// Configuracion general del sampleo de audio para
// la medida de ruido y deteccion de palmas.
// A 16kHz con 512 muestras cada buffer se llena en ~32 ms.
// Con N buffers, toleramos (N-1)*32 ms de latencia del main loop antes de perder datos.
// 4 buffers = 96 ms de tolerancia, suficiente para absorber I2C display + MQTT.
#define AUDIO_SAMPLE_BUFFER_SIZE 512
#define AUDIO_SAMPLE_NUM_BUFFERS 4
#define AUDIO_SAMPLE_RATE_HZ 16000
// buffer RTTTL para melodias
#define AUDIO_SONG_BUFFER_SIZE 256

#define SYSTEM_TIMEZONE 1

//Perro guardian para que no se cuelgue
//al perder conexion con el serivodr MQTT
#define WDT_ENABLE
#ifdef WDT_ENABLE
    #define USE_WWDT false
    #define USE_IWDT true
    #define WDT_TIMEOUT 10000
    #define WDT_TICK_PACE ((uint32_t)(WDT_TIMEOUT/8.0))
#endif

// Tiempo mínimo entre actualizaciones de T/RH hacia el CCS811 (ms)
#define CJMCU811_MIN_TIME_BETWEEN_ENVIRONMENTAL_DATA_UPDATE  (30000UL)
// Umbrales de cambio mínimo para refrescar datos ambientales
#define CJMCU811_TEMPERATURE_UPDATE_THRESHOLD                 (0.5f)  // ºC
#define CJMCU811_HUMIDITY_UPDATE_THRESHOLD                    (2.0f)  // %RH

// CONFIGURACION JSON PARSER
#define JSON_PARSER_COERCE_TYPES // Automatically convert types, for example number from string, if commented types are strict.

//OJO este polling lo hace internamente el sensor
//no es como el resto que los comprueba la aplicacion
//usamos el define para el resto de calculos y mantener la coherencia
#define CJMCU811_POLLING_INTERVAL 10000
#define BME280_POLLING_INTERVAL 4500
#define BH1750_POLLING_INTERVAL 5000
#define PMS5003_POLLING_INTERVAL 5500
#define MQTT_OUTPUT_INTERVAL 90000
#define MQTT_NOISE_OUTPUT_INTERVAL 60000
#define MQTT_RECONNECT_INTERVAL 10000
#define SYNC_TIME_INTERVAL 86400000//(24 * 60 * 60 * 1000)
#define HEALTH_TASK_INTERVAL 1000

#define DISPLAY_TASK_INTERVAL 50 // 50ms la task se encrga de planificr los subpasos del display

#define DISPLAY_TASK_FLUSH_MIN_GAP_MS  75U   // Tiempo minimo entre actualizaciones del display, no más de ~6-7 Hz
#define DISPLAY_TASK_FLUSH_MAX_WAIT_MS 300U   // Tiempo maximo entre actualizaciones del display
#define DISPLAY_TASK_TOP_EVERY_MS      1000U  // WiFi/hora/CO2 cabecera 
#define DISPLAY_TASK_MID_EVERY_MS      3000U  // PM grande
#define DISPLAY_TASK_BOT_EVERY_MS      1000U  // línea inferior (T/H/[LUX]/dB)

// AQI engine constants (solo se usan si ENABLE_AQI está definido)
#define AQI_MINUTE_MIN_SAMPLES   20   // nº de muestras requeridas para cerrar minuto
#define AQI_HOURLY_MIN_MINUTES   45   // nº de minutos válidos requeridos para cerrar hora
#define AQI_DAILY_MIN_HOURS      18   // nº de horas válidas requeridas para cerrar día

//Las medias estan calculadas para coincidir con
//el tiempo de envio del MQTT, eso quiere decir 
//que si el envio es cada 90 segundos, el valor
//enviado es la media de los ultimos 90 segundos.
#define PARTICLE_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / PMS5003_POLLING_INTERVAL)
#define TEMPERATURE_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / BME280_POLLING_INTERVAL)
#define HUMIDITY_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / BME280_POLLING_INTERVAL)
#define PRESSURE_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / BME280_POLLING_INTERVAL)
#define LUX_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / BH1750_POLLING_INTERVAL)
#define GAS_AVG_FACTOR (MQTT_OUTPUT_INTERVAL / CJMCU811_POLLING_INTERVAL)

#define MQTT_HOST "mqtt.example.local"
// #define MQTT_USE_TLS // Comment to disable tls, watch the port.
#define MQTT_PORT 1883 // 1883 without tls, 8883 tls

#define MQTT_OUT_BUFFER 640
#define MQTT_IN_BUFFER  384
#define MQTT_TOPIC_BUFFER 96
#define MQTT_LIB_MAX_PACKET_SIZE 768

#define MQTT_LWT_MESSAGE_OFF "Offline"
#define MQTT_LWT_MESSAGE_ON "Online"
#define MQTT_LWT_RETAIN true

//Subtopics to joing with global or base topics
#define MQTT_LWT_SUBTOPIC "/LWT"

#define MQTT_INFO_SUBTOPIC "/tele/INFO"

#define MQTT_SENSORS_SUBTOPIC "/tele/SENSOR"
#define MQTT_NOISE_SUBTOPIC "/tele/SENSOR/NOISE"
#define MQTT_CLAP_SUBTOPIC "/tele/SENSOR/CLAP"
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
#define MQTT_WHISTLE_SUBTOPIC "/tele/SENSOR/WHISTLE"
#endif
#define MQTT_PMS_STATE_SUBTOPIC "/tele/PMS_STATE"
#ifdef ENABLE_NEOPIXEL
#define MQTT_LED_CONTROL_SUBTOPIC "/cmd/LED"
#define MQTT_LED_STATUS_SUBTOPIC "/stat/LED"
#endif
#ifdef ENABLE_BUZZER
#define MQTT_BUZZER_STATUS_SUBTOPIC "/stat/BUZZER"
#define MQTT_BUZZER_CONTROL_SUBTOPIC "/cmd/BUZZER"
#define MQTT_BUZZER_PLAY_SUBTOPIC "/cmd/BUZZER/play"
#endif
#define MQTT_SYSTEM_REBOOT_SUBTOPIC "/cmd/REBOOT"
#define MQTT_SYSTEM_STAT_SUBTOPIC "/stat/SYSTEM"
#ifdef ENABLE_AQI
#define MQTT_AQI_SUBTOPIC   "/tele/SENSOR/AQI/"
#endif
// EXPERIMENTAL EN CURSO:
#define MQTT_OFFSETS_SET_SUBTOPIC     "/cmd/OFFSETS"      // payload JSON con 1..N claves
#define MQTT_OFFSETS_GET_SUBTOPIC     "/cmd/OFFSETS/get"  // payload vacío o "{}"
#define MQTT_OFFSETS_STATUS_SUBTOPIC  "/stat/OFFSETS"
// --- Audio calibration (mic noise floor)
#define MQTT_AUDIO_CAL_CMD_SUBTOPIC   "/cmd/AUDIO_CAL"   // {"action":"measure"} | {"action":"set","noise_floor":-45.0} | {"action":"get"}
#define MQTT_AUDIO_CAL_STATUS_SUBTOPIC "/stat/AUDIO_CAL"
// --- PMS duty-cycle config
#define MQTT_PMS_CFG_CMD_SUBTOPIC    "/cmd/PMS_CFG"
#define MQTT_PMS_CFG_STATUS_SUBTOPIC "/stat/PMS_CFG"
// --- Home Assistant Discovery control
#define MQTT_HA_CMD_SUBTOPIC      "/cmd/HA"
#define MQTT_HA_STATUS_SUBTOPIC   "/stat/HA"


//Device topics
#ifdef ENABLE_AQI
#define MQTT_AQI_BASE_TOPIC MQTT_BASE_TOPIC MQTT_AQI_SUBTOPIC
#endif
#define MQTT_LWT_TOPIC MQTT_BASE_TOPIC MQTT_LWT_SUBTOPIC
#define MQTT_INFO_TOPIC MQTT_BASE_TOPIC MQTT_INFO_SUBTOPIC
#define MQTT_SENSORS_TOPIC MQTT_BASE_TOPIC MQTT_SENSORS_SUBTOPIC
#define MQTT_NOISE_TOPIC MQTT_BASE_TOPIC MQTT_NOISE_SUBTOPIC
#define MQTT_CLAP_TOPIC MQTT_BASE_TOPIC MQTT_CLAP_SUBTOPIC
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
#define MQTT_WHISTLE_TOPIC MQTT_BASE_TOPIC MQTT_WHISTLE_SUBTOPIC
#endif
#define MQTT_PMS_STATE_TOPIC MQTT_BASE_TOPIC MQTT_PMS_STATE_SUBTOPIC
#ifdef ENABLE_NEOPIXEL
#define MQTT_LED_CONTROL_TOPIC MQTT_BASE_TOPIC MQTT_LED_CONTROL_SUBTOPIC
#define MQTT_LED_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_LED_STATUS_SUBTOPIC
#endif
#ifdef ENABLE_BUZZER
#define MQTT_BUZZER_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_BUZZER_STATUS_SUBTOPIC
#define MQTT_BUZZER_CONTROL_TOPIC MQTT_BASE_TOPIC MQTT_BUZZER_CONTROL_SUBTOPIC
#define MQTT_BUZZER_PLAY_TOPIC MQTT_BASE_TOPIC MQTT_BUZZER_PLAY_SUBTOPIC
#endif
#define MQTT_SYSTEM_REBOOT_TOPIC MQTT_BASE_TOPIC MQTT_SYSTEM_REBOOT_SUBTOPIC
#define MQTT_SYSTEM_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_SYSTEM_STAT_SUBTOPIC
#define MQTT_OFFSETS_SET_TOPIC MQTT_BASE_TOPIC MQTT_OFFSETS_SET_SUBTOPIC
#define MQTT_OFFSETS_GET_TOPIC MQTT_BASE_TOPIC MQTT_OFFSETS_GET_SUBTOPIC
#define MQTT_OFFSETS_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_OFFSETS_STATUS_SUBTOPIC
// --- Audio calibration ---
#define MQTT_AUDIO_CAL_CMD_TOPIC    MQTT_BASE_TOPIC MQTT_AUDIO_CAL_CMD_SUBTOPIC
#define MQTT_AUDIO_CAL_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_AUDIO_CAL_STATUS_SUBTOPIC
// --- PMS duty-cycle config ---
#define MQTT_PMS_CFG_CMD_TOPIC    MQTT_BASE_TOPIC MQTT_PMS_CFG_CMD_SUBTOPIC
#define MQTT_PMS_CFG_STATUS_TOPIC MQTT_BASE_TOPIC MQTT_PMS_CFG_STATUS_SUBTOPIC
// --- Home Assistant Discovery control (comando y estado) ---
#define MQTT_HA_CMD_TOPIC      MQTT_BASE_TOPIC MQTT_HA_CMD_SUBTOPIC
#define MQTT_HA_STATUS_TOPIC   MQTT_BASE_TOPIC MQTT_HA_STATUS_SUBTOPIC

//GLOBAL TOPICS
#ifdef ENABLE_BUZZER
#define MQTT_GLOBAL_BUZZER_PLAY_TOPIC  MQTT_GLOBAL_TOPIC MQTT_BUZZER_PLAY_SUBTOPIC
#endif
#ifdef ENABLE_NEOPIXEL
#define MQTT_GLOBAL_LED_CONTROL_TOPIC  MQTT_GLOBAL_TOPIC MQTT_LED_CONTROL_SUBTOPIC
#endif
#define MQTT_GLOBAL_REBOOT_TOPIC       MQTT_GLOBAL_TOPIC MQTT_SYSTEM_REBOOT_SUBTOPIC
#define MQTT_GLOBAL_HA_CMD_TOPIC       MQTT_GLOBAL_TOPIC MQTT_HA_CMD_SUBTOPIC

// HA TOPICS
#define MQTT_HA_DISCOVERY_PREFIX  "homeassistant"

//-----------------
// debug base configuration
//
//-----------------
#define DEBUG_SERIAL_PORT Serial
#define DEBUG_BAUD 115200


// SEGUROS Y CALCULOS - SACAR A OTRO FICHERO NO USER-CUSTOMIZABLE!

#if !defined(NEOPIXEL_LED_BRIGHTNESS) && defined(ENABLE_NEOPIXEL)
    #define NEOPIXEL_LED_BRIGHTNESS 50
#endif

// TODO: Sacar esto a otro fichero de config que no sea para tocar el usuario
#if defined(ENABLE_SERIAL_DEBUG) || defined(ENABLE_SERIAL_MQTT_DEBUG) || defined(ENABLE_MEGUNO_DEBUG)
    #define HAS_ANY_SERIAL_OUTPUT_ENABLED
#endif

// Ganancia software del micrófono (SoundMeter::X1..X16)
// Mics chinos necesitan X4; MAX4466 (con op-amp) → X1
#ifndef AUDIO_SOFTWARE_GAIN
    #define AUDIO_SOFTWARE_GAIN  SoundMeter::X4
#endif

#ifndef ACOUSTIC_NOTIFICATIONS_ENABLED
    #define ACOUSTIC_NOTIFICATIONS_ENABLED false
#endif

// Noise reporting: filtros IIR + publicación adaptativa
#ifndef NOISE_IIR_TAU_FAST_MS
    #define NOISE_IIR_TAU_FAST_MS    1500    // ~2s respuesta al nivel actual
#endif
#ifndef NOISE_IIR_TAU_SLOW_MS
    #define NOISE_IIR_TAU_SLOW_MS    45000   // ~30s referencia de ambiente
#endif
#ifndef NOISE_PUBLISH_DELTA_DB
    #define NOISE_PUBLISH_DELTA_DB   2.0f    // publicar si cambia >= 3 dB
#endif
#ifndef NOISE_MIN_INTERVAL_MS
    #define NOISE_MIN_INTERVAL_MS    2000    // mínimo entre publicaciones
#endif
#ifndef NOISE_MAX_INTERVAL_MS
    #define NOISE_MAX_INTERVAL_MS    60000   // máximo sin publicar (heartbeat)
#endif

// Validación CCS811 (eCO2 / TVOC)
#ifndef CCS811_CO2_MIN_PPM
    #define CCS811_CO2_MIN_PPM        400
#endif
#ifndef CCS811_CO2_MAX_PPM
    #define CCS811_CO2_MAX_PPM        5000
#endif
#ifndef CCS811_TVOC_MAX_PPB
    #define CCS811_TVOC_MAX_PPB       1187
#endif
#ifndef CCS811_SPIKE_FACTOR
    #define CCS811_SPIKE_FACTOR       3.0f
#endif
#ifndef CCS811_MAX_CONSEC_ERRORS
    #define CCS811_MAX_CONSEC_ERRORS  10
#endif
#ifndef CCS811_BASELINE_WARMUP_MS
    #define CCS811_BASELINE_WARMUP_MS   (20UL * 60 * 1000)  // 20 min antes de restaurar baseline
#endif
#ifndef CCS811_BASELINE_SAVE_MS
    #define CCS811_BASELINE_SAVE_MS     (24UL * 60 * 60 * 1000) // guardar cada 24h
#endif

// PMS duty-cycle defaults y validación
#ifndef PMS_DEFAULT_SLEEP_MIN
    #define PMS_DEFAULT_SLEEP_MIN     0     // 0 = continuo (activar duty-cycle via MQTT)
#endif
#ifndef PMS_DEFAULT_WAKE_SEC
    #define PMS_DEFAULT_WAKE_SEC      45
#endif
#ifndef PMS_WARMUP_SEC
    #define PMS_WARMUP_SEC            30
#endif
#ifndef PMS_PM25_MAX_UGM3
    #define PMS_PM25_MAX_UGM3         2000
#endif

#if defined(TEMP_SENSOR_BME280) || (defined(ENABLE_PARTICLE_SENSOR) && defined(PARTICLE_SENSOR_MODEL_PMS5003ST))
    #define HAS_HUMIDITY_DATA
#endif

#if defined(ENABLE_PARTICLE_SENSOR)
    #define HAS_PARTICLE_DATA
#endif

#if defined(ENABLE_LUX_SENSOR)
    #define HAS_LUX_DATA
#endif