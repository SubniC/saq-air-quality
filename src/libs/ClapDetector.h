#pragma once
#include "application.h"
#include "RunningAverageF.h"
#include "config/config.h"

namespace AUDIO {

struct ClapConfig {
  // General
  uint16_t sampleRateHz   = 16000;
  float    envAlpha       = 0.15f;

  // Clap FSM
  bool     enableClap     = true;
  float    kSigma         = 3.0f;
  float    kSigmaFalling  = 0.0f;  // Schmitt trigger: umbral de mantenimiento en RISING.
                                    // 0 = sin histeresis (usa kSigma). >0 = umbral mas bajo para sostener RISING.
  float    sigmaFloorDb   = 0.75f;
  int      peakMinMs      = 60;
  int      peakMaxMs      = 200;
  int      minGapMs       = 200;
  int      maxGapMs       = 0;     // Separacion maxima entre claps de una secuencia. 0 = sin limite.
  int      waitAfterFirstMs = 1000;
  int      waitAfterPrevMs  = 1500;
  int      debounceMs     = 2000;
  float    minDb          = -25.0f;
  float    relGateDb    = 6.0f;   // dB por encima del ambiente (SNR mínimo)
  uint16_t ambientTauMs = 1000;   // constante de tiempo de la media de ambiente (~1s)

  // Whistle (Goertzel)
  bool     enableWhistle  = false;
  uint16_t goertzelN      = 256;
  uint8_t  goertzelBins   = 10;
  uint16_t fMinHz         = 1500;
  uint16_t fMaxHz         = 4500;
  float    tonalityMin    = 0.65f;
  int      whistleMinMs   = 250;
    // --- filtros lógicos anti-falsos para whistle ---
  float    whistleDbExtraGate   = 8.0f;   // dBFS extra sobre cfg.minDb
  uint16_t whistleDebounceMs    = 500;    // silencio tras detectar
  uint16_t whistleGuardAfterClapMs = 1000; // mudo whistle tras hit de clap
  uint16_t whistleFreqStabilityHz = 200;  // pico estable +- este margen
  
};

class ClapDetector {
public:
  explicit ClapDetector(const ClapConfig& cfg);
  void begin();

  // Procesa un bloque: buf ADC (unsigned 12b), baseline DC, dB del bloque y duración del bloque (ms)
  void feedBlock(const uint16_t* buf, uint16_t len, float dc_baseline, float block_db, uint16_t block_time_ms);

  // Control
  void setClapEnabled(bool en)         { clapEnabled_ = en; }
  bool isClapEnabled()           const { return clapEnabled_; }
  void setWhistleEnabled(bool en)      { whistleEnabled_ = en; }
  bool isWhistleEnabled()        const { return whistleEnabled_; }

  // Calibración: aplica noise floor medido para mejorar bootstrap y gate dinámico
  void setNoiseFloor(float noise_floor_db);
  float getNoiseFloor()          const { return storedNoiseFloor_; }

  // Callbacks (firma compatible con SoundMeter)
  void setClapCallback(void (*cb)(uint8_t, unsigned long, float, float, float)) { clapCb_ = cb; }
  // duration_ms, peak_freq_hz, tonality [0..1], level_dbfs
  void setWhistleCallback(void (*cb)(unsigned long, uint16_t, float, float)) { whistleCb_ = cb; }

private:
  // Anti-falsos whistle
  uint32_t whistleAntibounceUntil_ = 0;
  uint32_t whistleGuardUntil_      = 0;     // mute tras clap
  uint16_t lastPeakHz_[3] = {0,0,0};        // historial de picos
  uint8_t  lastPeakIdx_   = 0;
  uint8_t preHitCount_ = 0;
  uint8_t ambientBootCount_ = 0;  // bloques silenciosos vistos antes de aceptar ambient
  float ambientDb_ = -90.0f;      // dBFS ambiente (EWMA)
  float storedNoiseFloor_ = 0.0f; // noise floor calibrado (0 = sin calibrar)
  float pulsePeakDb_ = -120.0f;

  // ---- Clap (envolvente + umbral + FSM) ----
  void updateEnvelope_(const uint16_t* buf, uint16_t len, float dc);
  // hit: umbral estricto (kSigma) para IDLE->RISING
  // hit_hold: umbral relajado (kSigmaFalling) para mantener RISING (Schmitt trigger)
  void stepClapFsm_(bool hit, bool hit_hold, float current_db, uint32_t now_ms, uint16_t block_time_ms);

  // ---- Whistle (Goertzel) ----
  void initGoertzel_();
  void processWhistle_(const uint16_t* buf, uint16_t len, float current_db, uint16_t block_time_ms);
  uint32_t goertzelPowerSum_(const int16_t* xb, uint16_t N, float& tonalityOut, uint16_t& peakFreqHzOut);

  // helper: devuelve true si pico estable ~ mismo Hz en las últimas 2 lecturas
  inline bool freqStable_(uint16_t hz, uint16_t tol) const {
    uint16_t a = lastPeakHz_[(lastPeakIdx_+2)%3];
    uint16_t b = lastPeakHz_[(lastPeakIdx_+1)%3];
    if (!a || !b) return false;
    uint16_t da = (hz>a)?(hz-a):(a-hz);
    uint16_t db = (hz>b)?(hz-b):(b-hz);
    return (da <= tol) && (db <= tol);
  }
private:
  ClapConfig cfg_;

  // Clap state
  bool clapEnabled_ = false;
  float env_ = 0.0f;
  RunningAverageF envAvg_{64};
  RunningAverageF envStd_{64};

  enum {IDLE, RISING, PEAK, FALLING, GAP} clapState_ = IDLE;
  uint32_t t0_ = 0, t1_ = 0;
  uint8_t  clapCount_ = 0;
  uint32_t firstStart_ = 0, currStart_ = 0, currEnd_ = 0, prevEnd_ = 0;
  uint32_t antibounceUntil_ = 0;

  void (*clapCb_)(uint8_t, unsigned long, float, float, float) = nullptr;

  // Whistle state
  bool whistleEnabled_ = false;
  bool goertzelInited_ = false;
  int16_t coefQ15_[16]; // máx 16 bins
  uint8_t binsUsed_ = 0;
  uint32_t whistleMsAcc_ = 0;

  void (*whistleCb_)(unsigned long, uint16_t, float, float) = nullptr;
};

} // namespace AUDIO
