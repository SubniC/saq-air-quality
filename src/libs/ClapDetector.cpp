#include "ClapDetector.h"
#include <cmath>
#include <cstring>
#include "utils/debug.h"


namespace AUDIO {

static inline const char* stName(uint8_t s) {
  switch(s){ case 0:return "IDLE"; case 1:return "RISING"; case 2:return "PEAK";
             case 3:return "FALLING"; case 4:return "GAP"; default:return "?"; }
}

#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
static inline int16_t coef_q15(uint16_t fs, uint16_t f0, uint16_t N) {
  float kf = (N * (float)f0) / (float)fs;
  int   k  = (int)(kf + 0.5f);
  float w  = 2.0f * 3.14159265359f * k / (float)N;
  float c  = 2.0f * cosf(w);
  int32_t q = (int32_t)lrintf(c * 32768.0f);
  if (q > 32767) q = 32767; else if (q < -32768) q = -32768;
  return (int16_t)q;
}
#endif // AUDIO_ENABLE_WHISTLE_GOERTZEL

ClapDetector::ClapDetector(const ClapConfig& cfg) : cfg_(cfg) {
  clapEnabled_    = cfg_.enableClap;
  whistleEnabled_ = cfg_.enableWhistle;

  LOG_DBG("{CLAP} ctor: clap=%d whistle=%d Fs=%u envAlpha=%.2f",
         (int)clapEnabled_, (int)whistleEnabled_, cfg_.sampleRateHz, cfg_.envAlpha);
  if (clapEnabled_) {
    LOG_DBG("{CLAP} params: kSigma=%.2f kFall=%.2f sigmaFloor=%.2f peak[%d..%d]ms gap[%d..%d] wait1=%d waitN=%d deb=%d minDb=%.1f",
           cfg_.kSigma, cfg_.kSigmaFalling, cfg_.sigmaFloorDb, cfg_.peakMinMs, cfg_.peakMaxMs,
           cfg_.minGapMs, cfg_.maxGapMs,
           cfg_.waitAfterFirstMs, cfg_.waitAfterPrevMs, cfg_.debounceMs, cfg_.minDb);
  }
  if (whistleEnabled_) {
    LOG_DBG("{WHIS} params: N=%u bins=%u f[ %u .. %u ]Hz tonality>=%.2f min=%dms",
           cfg_.goertzelN, cfg_.goertzelBins, cfg_.fMinHz, cfg_.fMaxHz, cfg_.tonalityMin, cfg_.whistleMinMs);
  }
}

void ClapDetector::begin() {
  #ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  if (whistleEnabled_) initGoertzel_();
  #endif
}

void ClapDetector::setNoiseFloor(float noise_floor_db) {
  storedNoiseFloor_ = noise_floor_db;
  // Si el noise floor calibrado es válido (no 0), usarlo para pre-seed del ambient
  // Esto mejora el bootstrap: en vez de esperar 3 bloques silenciosos,
  // el ambient arranca con un valor realista desde el primer momento.
  if (noise_floor_db < -10.0f && noise_floor_db > -90.0f) {
    ambientDb_ = noise_floor_db;
    ambientBootCount_ = 3;  // marcar como bootstrap completado
    LOG_DBG("{CLAP} noise floor calibrated: ambient pre-seeded to %.1f dBFS", noise_floor_db);
  }
}

void ClapDetector::feedBlock(const uint16_t* buf, uint16_t len, float dc, float block_db, uint16_t block_time_ms)
{
  // --- CLAP ---
  if (clapEnabled_) {
    const uint32_t now = millis();
    updateEnvelope_(buf, len, dc);        // actualiza env_ (envolvente del bloque)

    // 1) Lee estadísticas PREVIAS (sin contaminarlas con este bloque)
    float mu = envAvg_.getFastAverage();
    float sd = envStd_.GetStandardDeviation();
    if (sd < cfg_.sigmaFloorDb) sd = cfg_.sigmaFloorDb;

    // 2) Umbral estricto (kSigma) para IDLE->RISING
    const float thr = mu + cfg_.kSigma * sd;
    const bool  isHitCandidate = (env_ > thr);

    // 2b) Umbral relajado (kSigmaFalling) para mantener RISING (Schmitt trigger).
    //     Si kSigmaFalling == 0, usamos el mismo umbral (sin histeresis).
    const float thr_hold = (cfg_.kSigmaFalling > 0.0f)
                             ? (mu + cfg_.kSigmaFalling * sd)
                             : thr;
    const bool isHoldCandidate = (env_ > thr_hold);

    // 3) Bootstrap protegido del ambiente: requerir bloques silenciosos consecutivos
    //    antes de aceptar el primer valor como baseline. Evita contaminar con ruido
    //    fuerte si el dispositivo arranca durante aplausos o golpes.
    if (ambientDb_ < -80.0f) {
      if (block_db < -35.0f) {
        ambientBootCount_++;
        if (ambientBootCount_ >= 3) {
          ambientDb_ = block_db;
          LOG_DBG("{CLAP} ambient bootstrapped to %.1f dBFS after %u quiet blocks", block_db, ambientBootCount_);
        }
      } else {
        // Bloque ruidoso durante bootstrap: reiniciar contador
        ambientBootCount_ = 0;
      }
    }

    const float dynGate = max(cfg_.minDb, ambientDb_ + cfg_.relGateDb);
    const bool  hit      = isHitCandidate && (block_db >= dynGate);
    const bool  hit_hold = isHoldCandidate && (block_db >= dynGate);

    if (hit) {
      LOG_DBG("{CLAP} hit env=%.2f mu=%.2f sd=%.2f thr=%.2f dBFS=%.1f amb=%.1f gate=%.1f",
             env_, mu, sd, thr, block_db, ambientDb_, dynGate);
    }

    // 4) Actualiza ambiente SOLO si NO hay hit
    if (!hit) {
      const float tau = (cfg_.ambientTauMs > 0) ? (float)cfg_.ambientTauMs : 1.0f;
      float alpha = 1.0f - expf(-(float)block_time_ms / tau);
      if (alpha < 0.001f) alpha = 0.001f;
      if (alpha > 1.0f)   alpha = 1.0f;
      ambientDb_ = (1.0f - alpha) * ambientDb_ + alpha * block_db;
    }

    // 5) Actualiza μ/σ SOLO si NO hay hit y en estados de fondo (IDLE/GAP)
    if (!hit && (clapState_ == IDLE || clapState_ == GAP)) {
      envAvg_.addValue(env_);
      envStd_.addValue(env_);
    }

    // 6) Avanza la FSM. hit = umbral estricto, hit_hold = umbral relajado (Schmitt)
    stepClapFsm_(hit, hit_hold, block_db, now, block_time_ms);
  }

  // --- WHISTLE ---
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL
  if (whistleEnabled_) {
    processWhistle_(buf, len, block_db, block_time_ms);
  }
#endif
}

// =================== CLAP ===================

void ClapDetector::updateEnvelope_(const uint16_t* buf, uint16_t len, float dc)
{
  float e = env_;
  const float a = cfg_.envAlpha;
  for (uint16_t i=0;i<len;++i) {
    float x = (float)buf[i] - dc;
    float ax = fabsf(x);
    e = (1.0f - a)*e + a*ax;
  }
  env_ = e;
}

void ClapDetector::stepClapFsm_(bool hit, bool hit_hold, float current_db, uint32_t now, uint16_t /*block_time_ms*/)
{
  if (now < antibounceUntil_) {
    LOG_EVERY_MS(DBG, 500, "{CLAP} antibounce ...");
    return;
  }

  switch (clapState_) {
    case IDLE:
      // Usa umbral estricto (hit) para iniciar deteccion
      if (hit) {
        clapState_ = RISING; t0_ = now;
        pulsePeakDb_ = current_db;
        LOG_DBG("{CLAP} IDLE -> RISING t0=%lu dB=%.1f", t0_, current_db);
      }
      break;

    case RISING:
      // Schmitt trigger: usa hit_hold (umbral relajado) para mantener RISING.
      // Esto evita que senales cerca del umbral oscilen entre IDLE y RISING.
      if (hit_hold) {
        if (current_db > pulsePeakDb_) pulsePeakDb_ = current_db;
      } else {
        clapState_ = PEAK; t1_ = now;
        LOG_DBG("{CLAP} RISING -> PEAK t1=%lu dur=%lums", t1_, (t1_>=t0_)?(t1_-t0_):0);
      }
      break;

    case PEAK: {
      uint32_t dur = (t1_ >= t0_) ? (t1_ - t0_) : 0;
      clapState_ = FALLING;

      const bool valid = (dur >= (uint32_t)cfg_.peakMinMs &&
                          dur <= (uint32_t)cfg_.peakMaxMs &&
                          pulsePeakDb_ >= cfg_.minDb);
      LOG_DBG("{CLAP} PEAK -> FALLING dur=%lums valid=%d peakDb=%.1f (minDb=%.1f)",
             dur, (int)valid, pulsePeakDb_, cfg_.minDb);

      if (valid) {
        whistleGuardUntil_ = now + cfg_.whistleGuardAfterClapMs;

        if (clapCount_ > 0) {
          uint32_t gap = t0_ - currEnd_;
          if (gap < (uint32_t)cfg_.minGapMs) {
            LOG_DBG("{CLAP} gap too short (%lums < %dms) -> reset", gap, cfg_.minGapMs);
            clapCount_ = 0;
          }
          // maxGapMs: si el gap entre claps es demasiado largo, no son una secuencia
          else if (cfg_.maxGapMs > 0 && gap > (uint32_t)cfg_.maxGapMs) {
            LOG_DBG("{CLAP} gap too wide (%lums > %dms) -> sequence broken", gap, cfg_.maxGapMs);
            // Tratar este clap como inicio de nueva secuencia
            firstStart_ = t0_;
            currStart_  = t0_;
            currEnd_    = t1_;
            prevEnd_    = t1_;
            clapCount_  = 1;
          } else {
            prevEnd_ = currEnd_;
            currStart_ = t0_;
            currEnd_ = t1_;
            clapCount_++;
            LOG_DBG("{CLAP} clap++ -> %u", clapCount_);
          }
        } else {
          firstStart_ = t0_;
          currStart_  = t0_;
          currEnd_    = t1_;
          prevEnd_    = t1_;
          clapCount_  = 1;
          LOG_DBG("{CLAP} first clap (peakDb=%.1f)", pulsePeakDb_);
        }
      } else {
        pulsePeakDb_ = -120.0f;
      }
    } break;

    case FALLING:
      if (!hit) { clapState_ = GAP; LOG_DBG("{CLAP} FALLING -> GAP"); }
      break;

    case GAP: {
      // Nuevo hit durante espera: posible siguiente palmada de la secuencia.
      // Transicionar a RISING para capturar el nuevo pulso.
      if (hit && clapCount_ > 0) {
        uint32_t gap = now - currEnd_;
        if (gap >= (uint32_t)cfg_.minGapMs) {
          // Gap valido: capturar nueva palmada como parte de la secuencia
          clapState_ = RISING;
          t0_ = now;
          pulsePeakDb_ = current_db;
          LOG_DBG("{CLAP} GAP -> RISING (next clap, gap=%lums)", gap);
          break;
        }
        // gap < minGapMs: demasiado rapido, ignorar (eco/rebote)
      }

      if (clapCount_ > 0) {
        uint32_t elapsed = now - currEnd_;

        if (clapCount_ == 1 && elapsed >= (uint32_t)cfg_.waitAfterFirstMs) {
          LOG_DBG("{CLAP} single clap -> notify (elapsed=%lums)", elapsed);
          if (clapCb_) {
            const float peak   = pulsePeakDb_;
            const float amb    = ambientDb_;
            const float snr_db = peak - amb;
            clapCb_(1, currEnd_ - firstStart_, peak, amb, snr_db);
          }
          antibounceUntil_ = now + cfg_.debounceMs;
          clapCount_ = 0;
          pulsePeakDb_ = -120.0f;
        }
        else if (clapCount_ > 1 && elapsed >= (uint32_t)cfg_.waitAfterPrevMs) {
          LOG_DBG("{CLAP} sequence detected: %u claps, period=%lums", clapCount_, currEnd_ - firstStart_);
          if (clapCb_) {
            const float peak   = pulsePeakDb_;
            const float amb    = ambientDb_;
            const float snr_db = peak - amb;
            clapCb_(clapCount_, currEnd_ - firstStart_, peak, amb, snr_db);
          }
          antibounceUntil_ = now + cfg_.debounceMs;
          clapCount_ = 0;
          pulsePeakDb_ = -120.0f;
        }
      }

      if (!hit && clapCount_ == 0) {
        clapState_ = IDLE; LOG_DBG("{CLAP} GAP -> IDLE");
      }
    } break;
  }
}
// =================== WHISTLE ===================
#ifdef AUDIO_ENABLE_WHISTLE_GOERTZEL

void ClapDetector::initGoertzel_()
{
  binsUsed_ = (cfg_.goertzelBins > 16) ? 16 : cfg_.goertzelBins;
  for (uint8_t i=0;i<binsUsed_;++i) {
    float f = cfg_.fMinHz + i*(cfg_.fMaxHz - cfg_.fMinHz) / (float)(binsUsed_-1 ? binsUsed_-1 : 1);
    coefQ15_[i] = coef_q15(cfg_.sampleRateHz, (uint16_t)f, cfg_.goertzelN);
  }
  goertzelInited_ = true;
  LOG_DBG("{WHIS} Goertzel init: bins=%u", binsUsed_);
}

uint32_t ClapDetector::goertzelPowerSum_(const int16_t* xb, uint16_t N, float& tonalityOut, uint16_t& peakFreqHzOut)
{
  uint32_t total = 0;
  uint32_t maxp = 0;
  uint32_t maxIdx = 0;
  for (uint8_t i=0;i<binsUsed_;++i) {
    int16_t c = coefQ15_[i];
    int32_t q0, q1=0, q2=0;
    for (uint16_t n=0;n<N;++n) {
      q0 = ((int32_t)c * q1 >> 15) - q2 + (int32_t)xb[n];
      q2 = q1; q1 = q0;
    }
    int64_t p = (int64_t)q1*q1 + (int64_t)q2*q2 - ((int64_t)c*q1*q2 >> 15);
    if (p<0) p=0;
    uint32_t pu = (uint32_t)(p >> 8);
    total += pu;
    if (pu > maxp) { maxp = pu; maxIdx = i; }
  }
  tonalityOut = (total > 0) ? (float)maxp / (float)total : 0.0f;
  float f = cfg_.fMinHz + maxIdx * (cfg_.fMaxHz - cfg_.fMinHz) / (float)(binsUsed_-1 ? binsUsed_-1 : 1);
  peakFreqHzOut = (uint16_t)(f + 0.5f);
  return total;
}

void ClapDetector::processWhistle_(const uint16_t* buf, uint16_t len, float current_db, uint16_t block_time_ms)
{
  if (!goertzelInited_) initGoertzel_();
  
  const uint32_t now = millis();
  // Guard por clap reciente
  if (now < whistleGuardUntil_) {
    LOG_DBG("{WHIS} guard after clap (%lums left)", whistleGuardUntil_ - now);
    whistleMsAcc_ = 0;
    return;
  }

  // Debounce tras un whistle ya notificado
  if (now < whistleAntibounceUntil_) {
    LOG_DBG("{WHIS} debounce (%lums left)", whistleAntibounceUntil_ - now);
    whistleMsAcc_ = 0;
    return;
  }

  // Gating extra en nivel (dBFS)
  if (current_db < (cfg_.minDb + cfg_.whistleDbExtraGate)) {
    if (whistleMsAcc_) LOG_DBG("{WHIS} gating dBFS (%.1f < %.1f)", current_db, cfg_.minDb + cfg_.whistleDbExtraGate);
    whistleMsAcc_ = 0;
    return;
  }

  const uint16_t N = (cfg_.goertzelN <= len) ? cfg_.goertzelN : len;

  static int16_t xb[256]; // dimensionado al mayor AUDIO_GOERTZEL_N usado
  if (N > 256) return;    // seguridad

  for (uint16_t i=0;i<N;++i) {
    int32_t v = (int32_t)buf[i];
    v -= 2048;
    if (v>32767) v=32767; else if (v<-32768) v=-32768;
    xb[i] = (int16_t)v;
  }

  float tone = 0.0f; uint16_t peakHz = 0;
  uint32_t total = goertzelPowerSum_(xb, N, tone, peakHz);

  // Pre-seed del historial de frecuencias en el primer bloque con tonalidad valida.
  // Sin esto, freqStable_() rechaza los primeros 2 bloques (~64 ms) de un silbido
  // real porque el historial esta a cero.
  if (lastPeakIdx_ == 0 && tone >= cfg_.tonalityMin) {
    lastPeakHz_[0] = peakHz;
    lastPeakHz_[1] = peakHz;
    lastPeakHz_[2] = peakHz;
  }

  // actualiza historial de picos
  lastPeakHz_[ lastPeakIdx_ % 3 ] = peakHz;
  lastPeakIdx_++;

  bool stable = freqStable_(peakHz, cfg_.whistleFreqStabilityHz);

  if (total > 0 && tone >= cfg_.tonalityMin && stable) {
    whistleMsAcc_ += block_time_ms;
    if (whistleMsAcc_ >= (uint32_t)cfg_.whistleMinMs) {
      if (whistleCb_) whistleCb_(whistleMsAcc_, peakHz, tone, current_db);
      LOG_DBG("{WHIS} detected: dur=%lums freq=%uHz tone=%.2f dBFS=%.1f (stable)",
            whistleMsAcc_, peakHz, tone, current_db);
      whistleAntibounceUntil_ = now + cfg_.whistleDebounceMs;   // <<< debounce
      whistleMsAcc_ = 0;
    }
  } else {
    if (whistleMsAcc_ != 0) {
      LOG_DBG("{WHIS} reset (tone=%.2f stable=%d)", tone, (int)stable);
    }
    whistleMsAcc_ = 0;
  }
}
#endif // AUDIO_ENABLE_WHISTLE_GOERTZEL

} // namespace AUDIO
