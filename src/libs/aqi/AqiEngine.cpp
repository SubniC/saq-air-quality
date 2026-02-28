#include "AqiEngine.h"

namespace AQI {

// ===== Nowcast =====

void AqiEngine::Nowcast::pushHour(float conc) {
  head = (head + 1) % 12;
  hours[head] = conc;
  valid[head] = isfinite(conc) ? 1 : 0;
  if (n < 12) n++;
}

bool AqiEngine::Nowcast::compute(float& outConc) {
  // Se requieren ≥2 horas válidas.
  if (n < 2) return false;
  float cmin = INFINITY, cmax = -INFINITY;
  int   validCount = 0;
  // Recorremos de nueva a vieja (head, head-1, ...)
  for (uint8_t i = 0, idx = head; i < n; ++i) {
    if (valid[idx]) {
      float c = hours[idx];
      if (c < cmin) cmin = c;
      if (c > cmax) cmax = c;
      validCount++;
    }
    idx = (idx + 11) % 12;
  }
  if (validCount < 2 || !isfinite(cmin) || !isfinite(cmax) || cmax <= 0) return false;
  float ratio = cmin / cmax;
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  float w = ratio; if (w < 0.5f) w = 0.5f;

  // Media ponderada exponencial: hora 0 = más reciente (head)
  double num = 0.0, den = 0.0, wpow = 1.0;
  for (uint8_t i = 0, idx = head; i < n; ++i) {
    if (valid[idx]) {
      num += (double)hours[idx] * wpow;
      den += wpow;
    }
    wpow *= w;
    idx = (idx + 11) % 12;
  }
  if (den <= 0.0) return false;
  outConc = (float)(num / den);
  return isfinite(outConc);
}

// ===== Engine =====

AqiEngine::AqiEngine(const EngineConfig& cfg) : cfg_(cfg) {
  for (int k=0;k<2;k++) {
    ch_[k].m.reset(0);
    ch_[k].h.reset(0);
    ch_[k].d.reset(0);
  }
}

void AqiEngine::setCallback(EventCallback cb, void* ctx) {
  cb_ = cb; ctx_ = ctx;
}

float AqiEngine::truncateFor(Pollutant p, float ugm3) {
  return (p==Pollutant::PM25) ? AqiMapper::truncatePM25(ugm3) : AqiMapper::truncatePM10(ugm3);
}
int16_t AqiEngine::mapAQI(Pollutant p, float ugm3_trunc) {
  return (p==Pollutant::PM25) ? AqiMapper::aqiForPM25(ugm3_trunc) : AqiMapper::aqiForPM10(ugm3_trunc);
}

void AqiEngine::feedSample(Pollutant p, float conc_ugm3, uint32_t ts) {
  if (!isfinite(conc_ugm3) || conc_ugm3 < 0) return;

  const int k = (p==Pollutant::PM25)?0:1;
  auto& C = ch_[k];

  const uint32_t mIdx = idxMinuteLocal(ts);
  const uint32_t hIdx = idxHourLocal(ts);
  const uint32_t dIdx = idxDayLocal(ts);

  // Si es el primer sample, inicializa índices
  if (C.m.count==0 && C.h.minutesValid==0 && C.d.hoursValid==0) {
    C.m.reset(mIdx); C.h.reset(hIdx); C.d.reset(dIdx);
  }

  // ¿Cambia minuto?
  if (mIdx != C.m.minuteIndexLocal) {
    // Cierre del minuto anterior (fin = borde de minuto local)
    const uint32_t minuteEndLocal = C.m.minuteIndexLocal * 60;
    const uint32_t minuteEndTs    = (minuteEndLocal - (uint32_t)cfg_.tzOffsetSec);
    finalizeMinute(p, C.m.minuteIndexLocal, minuteEndTs);
    C.m.reset(mIdx);
  }
  // ¿Cambia hora?
  if (hIdx != C.h.hourIndexLocal) {
    const uint32_t hourEndLocal = C.h.hourIndexLocal * 3600;
    const uint32_t hourEndTs    = (hourEndLocal - (uint32_t)cfg_.tzOffsetSec);
    finalizeHour(p, C.h.hourIndexLocal, hourEndTs);
    C.h.reset(hIdx);
  }
  // ¿Cambia día?
  if (dIdx != C.d.dayIndexLocal) {
    const uint32_t dayEndLocal = C.d.dayIndexLocal * 86400;
    const uint32_t dayEndTs    = (dayEndLocal - (uint32_t)cfg_.tzOffsetSec);
    finalizeDay(p, C.d.dayIndexLocal, dayEndTs);
    C.d.reset(dIdx);
  }

  // Acumula en minuto
  C.m.sumConc += (double)conc_ugm3;
  C.m.count++;
}

void AqiEngine::finalizeMinute(Pollutant p, uint32_t minuteIdxLocal, uint32_t minuteEndTs) {
  const int k = (p==Pollutant::PM25)?0:1;
  auto& C = ch_[k];

  if (C.m.count >= cfg_.minMinuteSamples) {
    const float conc = (float)(C.m.sumConc / (double)C.m.count);
    const float tconc = truncateFor(p, conc);
    const int16_t aqi = mapAQI(p, tconc);

    C.last.concMinute = tconc; C.last.aqiMinute = aqi;

    // Sube a la hora
    C.h.sumMinuteConc += (double)tconc;
    C.h.minutesValid++;

    // Callback minuto
    if (cb_) { Event ev{p, Scope::Minute, minuteEndTs, tconc, aqi}; cb_(ctx_, ev); }
  }
  // si no valida, se ignora sin alterar hora
}

void AqiEngine::finalizeHour(Pollutant p, uint32_t hourIdxLocal, uint32_t hourEndTs) {
  const int k = (p==Pollutant::PM25)?0:1;
  auto& C = ch_[k];

  if (C.h.minutesValid >= cfg_.minHourlyMinutes) {
    const float concHour = (float)(C.h.sumMinuteConc / (double)C.h.minutesValid);
    const float tconc = truncateFor(p, concHour);
    const int16_t aqi = mapAQI(p, tconc);

    C.last.concHour = tconc; C.last.aqiHour = aqi;

    // Sube al día
    C.d.sumHourConc += (double)tconc;
    C.d.hoursValid++;

    // Callback hora
    if (cb_) { Event ev{p, Scope::Hour, hourEndTs, tconc, aqi}; cb_(ctx_, ev); }

    // NowCast (requiere ≥ 2 horas válidas en ventana)
    C.nc.pushHour(tconc);
    float ncConc;
    if (C.nc.compute(ncConc)) {
      const float ntc = truncateFor(p, ncConc);
      const int16_t naq = mapAQI(p, ntc);
      C.last.concNow = ntc; C.last.aqiNow = naq;
      if (cb_) { Event ev{p, Scope::NowCast, hourEndTs, ntc, naq}; cb_(ctx_, ev); }
    }
  }
}

void AqiEngine::finalizeDay(Pollutant p, uint32_t dayIdxLocal, uint32_t dayEndTs) {
  const int k = (p==Pollutant::PM25)?0:1;
  auto& C = ch_[k];

  if (C.d.hoursValid >= cfg_.minDailyHours) {
    const float concDay = (float)(C.d.sumHourConc / (double)C.d.hoursValid);
    const float tconc = truncateFor(p, concDay);
    const int16_t aqi = mapAQI(p, tconc);

    C.last.concDay = tconc; C.last.aqiDay = aqi;

    // Callback día
    if (cb_) { Event ev{p, Scope::Day, dayEndTs, tconc, aqi}; cb_(ctx_, ev); }
  }
}

bool AqiEngine::getLastMinute(Pollutant p, float& conc, int16_t& aqi) const {
  const int k = (p==Pollutant::PM25)?0:1;
  conc = ch_[k].last.concMinute; aqi = ch_[k].last.aqiMinute;
  return isfinite(conc) && aqi>=0;
}
bool AqiEngine::getLastHour(Pollutant p, float& conc, int16_t& aqi) const {
  const int k = (p==Pollutant::PM25)?0:1;
  conc = ch_[k].last.concHour; aqi = ch_[k].last.aqiHour;
  return isfinite(conc) && aqi>=0;
}
bool AqiEngine::getLastDay(Pollutant p, float& conc, int16_t& aqi) const {
  const int k = (p==Pollutant::PM25)?0:1;
  conc = ch_[k].last.concDay; aqi = ch_[k].last.aqiDay;
  return isfinite(conc) && aqi>=0;
}
bool AqiEngine::getLastNowcast(Pollutant p, float& conc, int16_t& aqi) const {
  const int k = (p==Pollutant::PM25)?0:1;
  conc = ch_[k].last.concNow; aqi = ch_[k].last.aqiNow;
  return isfinite(conc) && aqi>=0;
}

} // namespace AQI
