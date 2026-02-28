#pragma once

#include "application.h"
#include "config/config.h"

#if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
  #include "ClapDetector.h"
  #include "utils/placement_new.h"
#endif

class SoundMeter {
public:
    enum SoundGains_t {X1=0,X2=1,X4=2,X8=3,X16=4};

    SoundMeter(uint16_t sample_freq, uint16_t buffer_len);
    ~SoundMeter();

    void begin(void);
    void parseBucket(uint16_t *bucket, uint16_t lenght);
    void processBucket(void);

    // Callbacks
    // Noise: (dbfs_fast, dbfs_slow)
    void addNoiseLevelCallback(void (*callback)(float, float));
    void addClapCallback(void (*callback)(uint8_t, unsigned long, float, float, float));
    void addWhistleCallback(void (*callback)(unsigned long, uint16_t, float, float));

    // Nivel actual (IIR rápido ~2s)
    float getNoiseLevel() const { return _iir_fast; }
    // Nivel ambiente (IIR lento ~30s)
    float getAmbientLevel() const { return _iir_slow; }

    float getdBfromRMS(float rms);

    void enableClapDetector();
    void disableClapDetector();
    bool isEnabledClapDetector();

    void enableWhistleDetector();
    void disableWhistleDetector();
    bool isEnabledWhistleDetector();

    // Calibración de micrófono: mide el nivel de silencio (noise floor)
    void startNoiseFloorMeasurement(uint16_t duration_seconds = 5);
    bool isNoiseFloorMeasuring() const { return _cal_active; }
    void applyNoiseFloor(float noise_floor_db);
    void setCalibrationCallback(void (*cb)(float)) { _calCallback = cb; }

private:
    // ===== Hardware/calibración =====
    const uint16_t _MEASURED_VCC  = 3297;
    const uint16_t _MEASURED_VREF = 2497;
    const uint16_t _ADC_MAX_VALUE = 4095;

    const uint16_t _VERF_ADC_TICKS_33;
    const uint16_t _sample_freq;       // Hz
    const uint16_t _sample_time;       // ms del bloque
    const uint16_t _1_sec_avg_items;   // bloques por segundo
    SoundGains_t _software_gain;

    #if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
      InPlace<AUDIO::ClapDetector> _clap_storage;
      AUDIO::ClapDetector* _clap = nullptr; // alias rápido a _clap_storage.get()
    #endif

    bool      _is_init = false;
    uint16_t *_workingBuffer;
    uint16_t  _workingBufferLenght;

    // Último dB de bloque (dBFS)
    float last_dB = 0.0f;

    uint16_t _max_signal_amplitude;

    // Baseline DC de ADC (IIR lento)
    float _dc_baseline = (float)_ADC_MAX_VALUE * 0.5f;
    float _dc_alpha    = 0.001f;   // muy lento

    // ===== Noise IIR + publicación adaptativa =====
    float    _iir_fast = -90.0f;        // nivel actual (~2s tau)
    float    _iir_slow = -90.0f;        // nivel ambiente (~30s tau)
    float    _last_published_db = -999.0f; // último valor enviado por MQTT
    uint32_t _last_publish_ms = 0;      // timestamp última publicación

    // Callback de ruido: (dbfs_fast, dbfs_slow)
    void (*noiseCallback)(float, float) = nullptr;

    // Calibración noise floor
    void (*_calCallback)(float) = nullptr;
    bool     _cal_active = false;
    uint16_t _cal_blocks_remaining = 0;
    uint16_t _cal_blocks_total = 0;
    float    _cal_sum_db = 0.0f;

    // Cálculos
    float _calculate_RMS(void);
    float _calculate_dB(void);

    void  _update_baseline_IIR();
    void  _update_noise_iir();
};
