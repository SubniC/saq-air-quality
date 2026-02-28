#include "SoundMeter.h"
#include <math.h>
#include "utils/debug.h"

// ===== Construcción y setup =====
SoundMeter::SoundMeter(uint16_t Samplefreq, uint16_t buffer_lenght)
: _VERF_ADC_TICKS_33((_MEASURED_VREF * _ADC_MAX_VALUE) / _MEASURED_VCC),
  _sample_freq(Samplefreq),
  _sample_time((uint16_t)((1000.0f/(float)Samplefreq)*buffer_lenght)),
  _1_sec_avg_items(1000/_sample_time),
  _software_gain(AUDIO_SOFTWARE_GAIN){

#if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
    {
        AUDIO::ClapConfig c;
        c.sampleRateHz = _sample_freq;

    #ifdef AUDIO_ENABLE_DETECT_CLAPS
        c.enableClap      = true;
        c.kSigma          = AUDIO_CLAP_K_SIGMA;
        c.sigmaFloorDb    = AUDIO_CLAP_SIGMA_FLOOR_DB;
        c.peakMinMs       = AUDIO_CLAP_PEAK_MIN_MS;
        c.peakMaxMs       = AUDIO_CLAP_PEAK_MAX_MS;
        c.minGapMs        = AUDIO_CLAP_MIN_TIME_BETWEEN_MS;
        c.waitAfterFirstMs= AUDIO_CLAP_WAIT_AFTER_FIRST_MS;
        c.waitAfterPrevMs = AUDIO_CLAP_WAIT_AFTER_PREV_MS;
        c.debounceMs      = AUDIO_CLAP_DEBOUNCE_MS;
        c.minDb           = AUDIO_CLAP_TRIGGER_MIN_DB;
        c.relGateDb       = AUDIO_CLAP_REL_GATE_DB;
        c.ambientTauMs    = AUDIO_CLAP_AMBIENT_TAU_MS;
        #ifdef AUDIO_CLAP_MAX_TIME_BETWEEN_MS
        c.maxGapMs        = AUDIO_CLAP_MAX_TIME_BETWEEN_MS;
        #endif
        #ifdef AUDIO_CLAP_K_SIGMA_FALLING
        c.kSigmaFalling   = AUDIO_CLAP_K_SIGMA_FALLING;
        #endif
    #else
        c.enableClap      = false;
    #endif

    #ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
        c.enableWhistle   = true;
        c.goertzelN       = AUDIO_GOERTZEL_N;
        c.goertzelBins    = AUDIO_GOERTZEL_BINS;
        c.fMinHz          = AUDIO_GOERTZEL_FMIN_HZ;
        c.fMaxHz          = AUDIO_GOERTZEL_FMAX_HZ;
        c.tonalityMin     = AUDIO_WHISTLE_TONALITY_MIN;
        c.whistleMinMs    = AUDIO_WHISTLE_MIN_MS;
        c.whistleDbExtraGate      = AUDIO_WHISTLE_DB_EXTRA_GATE;
        c.whistleDebounceMs       = AUDIO_WHISTLE_DEBOUNCE_MS;
        c.whistleGuardAfterClapMs = AUDIO_WHISTLE_GUARD_AFTER_CLAP_MS;
        c.whistleFreqStabilityHz  = AUDIO_WHISTLE_FREQ_STABILITY_HZ;
    #else
        c.enableWhistle   = false;
    #endif

        _clap = _clap_storage.construct(c);
        _clap->begin();
        LOG_DBG("{SND} init Fs=%u Hz block=%u ms 1sItems=%u", _sample_freq, _sample_time, _1_sec_avg_items);
    }
#else
    LOG_DBG("{SND} init Fs=%u Hz block=%u ms (sin clap/whistle)", _sample_freq, _sample_time);
#endif

    _max_signal_amplitude = (_ADC_MAX_VALUE/2) << (int)_software_gain;
}

SoundMeter::~SoundMeter() {
#if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
    _clap_storage.destroy();
    _clap = nullptr;
#endif
}

void SoundMeter::enableClapDetector()  {
#ifdef AUDIO_ENABLE_DETECT_CLAPS
  if (_clap) _clap->setClapEnabled(true);
  LOG_DBG("{SND} clap enabled");
#endif
}
void SoundMeter::disableClapDetector() {
#ifdef AUDIO_ENABLE_DETECT_CLAPS
  if (_clap) _clap->setClapEnabled(false);
  LOG_DBG("{SND} clap disabled");
#endif
}
bool SoundMeter::isEnabledClapDetector(){
    #ifdef AUDIO_ENABLE_DETECT_CLAPS
        if (_clap) {return _clap->isClapEnabled();}
    #endif
    return false;
}

void SoundMeter::enableWhistleDetector()  {
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  if (_clap) _clap->setWhistleEnabled(true);
  LOG_DBG("{SND} whistle enabled");
#endif
}
void SoundMeter::disableWhistleDetector() {
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  if (_clap) _clap->setWhistleEnabled(false);
  LOG_DBG("{SND} whistle disabled");
#endif
}
bool SoundMeter::isEnabledWhistleDetector(){
    #ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
        if (_clap) {return _clap->isWhistleEnabled();}
    #endif
    return false;
}

void SoundMeter::addNoiseLevelCallback(void (*callback)(float, float)) { noiseCallback = callback; }
void SoundMeter::addClapCallback(void (*callback)(uint8_t, unsigned long, float, float, float)) {
#if defined(AUDIO_ENABLE_DETECT_CLAPS)
    if (_clap) _clap->setClapCallback(callback);
#endif
}
void SoundMeter::addWhistleCallback(void (*callback)(unsigned long, uint16_t, float, float)) {
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  if (_clap) _clap->setWhistleCallback(callback);
#endif
}

void SoundMeter::begin() { _is_init = true; }

// ===== Pipeline por bloque =====
void SoundMeter::parseBucket(uint16_t *bucket, uint16_t lenght)
{
    _workingBuffer = bucket;
    _workingBufferLenght = lenght;

    // 1) Actualiza baseline DC con IIR lento (para centrar)
    _update_baseline_IIR();

    // 2) Calcula dB del bloque (RMS seguro)
    last_dB = _calculate_dB();  // dBFS por bloque

#if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
    if (_clap) _clap->feedBlock(_workingBuffer, _workingBufferLenght, _dc_baseline, last_dB, _sample_time);
#endif
}

void SoundMeter::processBucket(void)
{
    // Calibración: acumula lecturas de silencio
    if (_cal_active) {
        _cal_sum_db += last_dB;
        if (--_cal_blocks_remaining == 0) {
            _cal_active = false;
            float avg = _cal_sum_db / (float)_cal_blocks_total;
            LOG_INFO("{SND} noise floor measurement done: %.1f dBFS (%u blocks)", avg, _cal_blocks_total);
            applyNoiseFloor(avg);
            if (_calCallback) _calCallback(avg);
        }
    }

    // Actualiza filtros IIR y publica adaptativamente si procede
    _update_noise_iir();
}

// ===== Cálculos base =====
float SoundMeter::_calculate_RMS()
{
    float acc = 0.0f;
    for(uint16_t i=0; i<_workingBufferLenght; ++i) {
        float a = (float)_workingBuffer[i] - _dc_baseline;
        a = ldexpf(a, _software_gain);
        acc += a*a;
    }
    acc /= (float)_workingBufferLenght;
    return sqrtf(acc);
}

float SoundMeter::_calculate_dB()
{
    return 20.0f * log10f(_calculate_RMS() / (float)_max_signal_amplitude);
}

float SoundMeter::getdBfromRMS(float rms)
{
    return 20.0f * log10f(rms / (float)_max_signal_amplitude);
}

void SoundMeter::_update_baseline_IIR()
{
    for (uint16_t i=0; i<_workingBufferLenght; ++i) {
        float x = (float)_workingBuffer[i];
        _dc_baseline = (1.0f - _dc_alpha)*_dc_baseline + _dc_alpha*x;
    }
}

// ===== Noise IIR + publicación adaptativa =====
void SoundMeter::_update_noise_iir()
{
    const float dt = (float)_sample_time;  // ms del bloque

    // Coeficientes IIR: alpha = 1 - exp(-dt/tau)
    const float a_fast = 1.0f - expf(-dt / (float)NOISE_IIR_TAU_FAST_MS);
    const float a_slow = 1.0f - expf(-dt / (float)NOISE_IIR_TAU_SLOW_MS);

    // Primer bloque: seed directo
    if (_iir_fast < -89.0f) {
        _iir_fast = last_dB;
        _iir_slow = last_dB;
    } else {
        _iir_fast = (1.0f - a_fast) * _iir_fast + a_fast * last_dB;
        _iir_slow = (1.0f - a_slow) * _iir_slow + a_slow * last_dB;
    }

    // Publicación adaptativa
    if (!noiseCallback) return;

    const uint32_t now = millis();
    const uint32_t elapsed = now - _last_publish_ms;
    const float    delta   = fabsf(_iir_fast - _last_published_db);

    bool should_publish = false;

    // Publicar si: cambio significativo Y cooldown mínimo cumplido
    if (delta >= NOISE_PUBLISH_DELTA_DB && elapsed >= NOISE_MIN_INTERVAL_MS) {
        should_publish = true;
    }
    // Publicar si: intervalo máximo excedido (heartbeat)
    else if (elapsed >= NOISE_MAX_INTERVAL_MS) {
        should_publish = true;
    }

    if (should_publish) {
        _last_published_db = _iir_fast;
        _last_publish_ms = now;
        noiseCallback(_iir_fast, _iir_slow);

        LOG_DBG("{NOISE} pub fast=%.1f slow=%.1f Δ=%.1f elapsed=%lums",
                _iir_fast, _iir_slow, delta, elapsed);
    }
}

// ===== Calibración de micrófono =====
void SoundMeter::startNoiseFloorMeasurement(uint16_t duration_seconds)
{
    _cal_blocks_total = duration_seconds * _1_sec_avg_items;
    _cal_blocks_remaining = _cal_blocks_total;
    _cal_sum_db = 0.0f;
    _cal_active = true;
    LOG_INFO("{SND} noise floor measurement started: %u s (%u blocks)", duration_seconds, _cal_blocks_total);
}

void SoundMeter::applyNoiseFloor(float noise_floor_db)
{
#if defined(AUDIO_ENABLE_DETECT_CLAPS) || defined(AUDIO_ENABLE_WHISTLE_GOERTZEL)
    if (_clap) {
        _clap->setNoiseFloor(noise_floor_db);
        LOG_INFO("{SND} noise floor applied to detector: %.1f dBFS", noise_floor_db);
    }
#endif
}
