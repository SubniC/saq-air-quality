#pragma once

    #define AIR_QUALITY_ID 1

    #define EEPROM_CONFIG_VERSION 1
    //#############################################################
    //# CONFIGURACION DE FUNCIONALIDADES
    //#############################################################

    //Utilizar o no el LCD
    #define ENABLE_LCD

    //Flag de habilitacion del sensor de lumenes
    // #define ENABLE_LUX_SENSOR

    // #define ENABLE_BUZZER
    #define ACOUSTIC_NOTIFICATIONS_ENABLED false
    #define BUZZER_PIN WKP


    //Flag de habilitacion y configuracion del sensor de particulas
    #define ENABLE_PARTICLE_SENSOR
    #ifdef ENABLE_PARTICLE_SENSOR
        //#define PARTICLE_SENSOR_MODEL_PMS5003
        #define PARTICLE_SENSOR_MODEL_PMS5003ST
        // #define PARTICLE_SENSOR_MODEL_PMS7003
    #endif

    //Utilizar o no Neopixel leds
    // #define ENABLE_NEOPIXEL
    // #define AUDIO_CLAP_WHISLE_VISUAL_DEBUG

    //#############################################################
    //# FLAGS DE DEPURACION
    //#############################################################

    //Sacar info de depurracion por serie
    #define ENABLE_SERIAL_DEBUG
    #ifdef ENABLE_SERIAL_DEBUG
        //Perfilar tiempos del loop
        // #define PROFILE_TIMES
    #endif

    //Sacar datos de sensores por serie
    // #define ENABLE_MEGUNO_TIMEPLOT_DEBUG


    //#############################################################
    //# PINES
    //#############################################################

    #define NEOPIXEL_DATA_PIN D5
    #define CJMCU811_INT_PIN D6
    #define PMS_ENABLE_PIN A5
    #define MIC_PIN A1
    #define LCD_PIN WKP

    //#############################################################
    //# CONFIGURACION DE DISPOSITIVOS
    //#############################################################

    #define LCD_I2C_ADDRESS 0x3C
    #define BME280_I2C_ADDRESS 0x76
    //Utilizar sensor externo de temperatura para el CCS811
    #define CJMCU811_EXTERNAL_TEMPERATURE
    //Numero de leds conectados
    #define NEOPIXEL_LED_COUNT 12

    // #define TEMP_SENSOR_BMP280
    #define TEMP_SENSOR_BME280

    //#############################################################
    //# FUNCIONALIDAD EXPERIMENTAL
    //#############################################################

    //Detección de palmas
    #define AUDIO_ENABLE_DETECT_CLAPS
    #define AUDIO_ENABLE_WHISTLE_GOERTZEL 

        // ===== Clap detector (FSM) =====
        // Ver AUDIO_DETECTION_ANALYSIS.md para justificacion de cada valor.
    #ifdef AUDIO_ENABLE_DETECT_CLAPS
        #define AUDIO_CLAP_REL_GATE_DB             8.0f
        #define AUDIO_CLAP_AMBIENT_TAU_MS          1000
        #define AUDIO_CLAP_K_SIGMA                 2.5f    // umbral entrada: mu + 2.5*sigma
        #define AUDIO_CLAP_K_SIGMA_FALLING         2.0f    // umbral mantenimiento (Schmitt trigger)
        #define AUDIO_CLAP_SIGMA_FLOOR_DB          3.0
        #define AUDIO_CLAP_TRIGGER_MIN_DB         -22.0f   // filtrar ruido domestico
        #define AUDIO_CLAP_PEAK_MIN_MS             25      // palmas rapidas en sala
        #define AUDIO_CLAP_PEAK_MAX_MS             250     // margen para reverberacion
        #define AUDIO_CLAP_MIN_TIME_BETWEEN_MS     140
        #define AUDIO_CLAP_MAX_TIME_BETWEEN_MS     1200    // gap maximo entre claps de una secuencia
        #define AUDIO_CLAP_WAIT_AFTER_FIRST_MS     650
        #define AUDIO_CLAP_WAIT_AFTER_PREV_MS      700
        #define AUDIO_CLAP_DEBOUNCE_MS             500
    #endif

    // ===== Silbidos (opcional con Goertzel) =====
    #ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
        #define AUDIO_GOERTZEL_N                   256
        #define AUDIO_GOERTZEL_BINS                12
        #define AUDIO_GOERTZEL_FMIN_HZ             2000
        #define AUDIO_GOERTZEL_FMAX_HZ             5500
        #define AUDIO_WHISTLE_TONALITY_MIN         0.75f   // silbidos humanos tienen armonicos
        #define AUDIO_WHISTLE_MIN_MS               300     // deteccion mas rapida
        #define AUDIO_WHISTLE_DB_EXTRA_GATE        8.0f    // +8 dB por encima del minDb de clap
        #define AUDIO_WHISTLE_DEBOUNCE_MS          800
        #define AUDIO_WHISTLE_GUARD_AFTER_CLAP_MS  350     // guard cortito, solo en clap válido
        #define AUDIO_WHISTLE_FREQ_STABILITY_HZ    100     // pico estable ±100 Hz
    #endif

    //ADC COMPENSATION
    //La idea es usar la referencia de voltaje de 2.5v que tenemos conectada a A3
    //para compensar los resultados del ADC contra un valor conocido.
    //, para revisar en el furuto
    //
    // Para compensar elADC, mediremos el valor de la referencia despues de medir el
    // valor del canal que nos interesa y aplicaremos la siguiente formula:
    // LECTURA_CANLAL * LECTURA_CANAL_VREF / ( 3102 )
    //
    // 3102 es el numero de ticks que un ADC ideal de 3.3v y 12bits daria para la referencia de 2.5v
    //
    //  3.3v -> 4095 ticks
    //  2.5v -> X ticks
    //  (2.5 * 4095 ) / 3.3 = 3102
    //#define SYSTEM_COMPENSATE_ADC
    #ifdef SYSTEM_COMPENSATE_ADC
        #define VREF_PIN A3
    #endif

    //#############################################################
    //# Configuracion MQTT
    //#############################################################

    //Configuracion del MQTT especifica del dispositivo
    #define MQTT_CLIENT_ID "saq1"
    #define MQTT_USER "saq1"
    #define MQTT_PASSWD "CHANGE_ME"
    #define MQTT_BASE_TOPIC "homebot/SAQ-1"
    #define MQTT_GLOBAL_TOPIC "homebot/saqs"
    #define MQTT_LISTEN_TO_GLOBAL true

    // #define MQTT_LWT_TOPIC "homebot/SAQ-1/LWT"
    // #define MQTT_SENSORS_TOPIC "homebot/SAQ-1/tele/SENSOR"
    // #define MQTT_NOISE_TOPIC "homebot/SAQ-1/tele/SENSOR/NOISE"
    // #define MQTT_CLAP_TOPIC "homebot/SAQ-1/tele/SENSOR/CLAP"
    // #define MQTT_AQI_TOPIC "homebot/SAQ-1/tele/SENSOR/AQI"
    // #define MQTT_LED_CONTROL_TOPIC "homebot/SAQ-1/cmd/LED"
    // #define MQTT_LED_STATUS_TOPIC "homebot/SAQ-1/stat/LED"
    // #define MQTT_BUZZER_STATUS_TOPIC "homebot/SAQ-1/stat/BUZZER"
    // #define MQTT_BUZZER_CONTROL_TOPIC "homebot/SAQ-1/cmd/BUZZER"
