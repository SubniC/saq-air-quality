#pragma once
#include "application.h"
#include <math.h>
#include "AqiMapper.h"
#include "config/config.h"

// Motor de buckets alineados (min/hora/día) + NowCast, con callbacks.
// Diseño sin dinámica y con getters de “último consolidado” por bucket.

namespace AQI {

enum class Scope : uint8_t { Minute=0, Hour=1, Day=2, NowCast=3 };

struct Event {
  Pollutant pollutant;
  Scope     scope;
  uint32_t  ts;     // epoch (segundos) del cierre del cubo
  float     conc;   // µg/m³ (tras truncado EPA que corresponda a p)
  int16_t   aqi;    // [0..500], -1 si inválido
};

typedef void (*EventCallback)(void* ctx, const Event&);

struct EngineConfig {
  int32_t tzOffsetSec = 0;
  uint16_t minMinuteSamples = AQI_MINUTE_MIN_SAMPLES;
  uint8_t  minHourlyMinutes = AQI_HOURLY_MIN_MINUTES;
  uint8_t  minDailyHours    = AQI_DAILY_MIN_HOURS;
};

class AqiEngine {
 public:
  explicit AqiEngine(const EngineConfig& cfg = EngineConfig());

  void setCallback(EventCallback cb, void* ctx);

  // Alimentar nueva muestra (µg/m³) con timestamp epoch (segundos).
  void feedSample(Pollutant p, float conc_ugm3, uint32_t ts);

  // Getters (último consolidado). Devuelven false si no hay dato válido aún.
  bool getLastMinute(Pollutant p, float& conc, int16_t& aqi) const;
  bool getLastHour  (Pollutant p, float& conc, int16_t& aqi) const;
  bool getLastDay   (Pollutant p, float& conc, int16_t& aqi) const;
  bool getLastNowcast(Pollutant p, float& conc, int16_t& aqi) const;

 private:
  struct MinuteAgg {
    uint32_t minuteIndexLocal = 0;
    uint32_t count = 0;
    double   sumConc = 0.0;
    void reset(uint32_t idx) { minuteIndexLocal = idx; count = 0; sumConc = 0.0; }
  };
  struct HourAgg {
    uint32_t hourIndexLocal = 0;
    uint8_t  minutesValid = 0;
    double   sumMinuteConc = 0.0;
    void reset(uint32_t idx) { hourIndexLocal = idx; minutesValid = 0; sumMinuteConc = 0.0; }
  };
  struct DayAgg {
    uint32_t dayIndexLocal = 0;
    uint8_t  hoursValid = 0;
    double   sumHourConc = 0.0;
    void reset(uint32_t idx) { dayIndexLocal = idx; hoursValid = 0; sumHourConc = 0.0; }
  };
  struct LastVals {
    float   concMinute = NAN; int16_t aqiMinute = -1;
    float   concHour   = NAN; int16_t aqiHour   = -1;
    float   concDay    = NAN; int16_t aqiDay    = -1;
    float   concNow    = NAN; int16_t aqiNow    = -1;
  };
  struct Nowcast {
    // Buffer circular de 12 horas (concentración media por hora)
    float    hours[12]; uint8_t valid[12]; uint8_t n=0; uint8_t head=0;
    Nowcast() { for (int i=0;i<12;i++){hours[i]=NAN; valid[i]=0;} }
    void pushHour(float conc);
    bool compute(float& outConc);
  };

  struct Channel {
    MinuteAgg  m;
    HourAgg    h;
    DayAgg     d;
    Nowcast    nc;
    LastVals   last;
  };

 private:
  EngineConfig cfg_;
  EventCallback cb_ = nullptr;
  void* ctx_ = nullptr;
  Channel ch_[2]; // PM25, PM10

  inline uint32_t idxMinuteLocal(uint32_t ts) const { return (ts + cfg_.tzOffsetSec) / 60; }
  inline uint32_t idxHourLocal  (uint32_t ts) const { return (ts + cfg_.tzOffsetSec) / 3600; }
  inline uint32_t idxDayLocal   (uint32_t ts) const { return (ts + cfg_.tzOffsetSec) / 86400; }

  void finalizeMinute(Pollutant p, uint32_t minuteIdxLocal, uint32_t minuteEndTs);
  void finalizeHour  (Pollutant p, uint32_t hourIdxLocal,   uint32_t hourEndTs);
  void finalizeDay   (Pollutant p, uint32_t dayIdxLocal,    uint32_t dayEndTs);

  static float truncateFor(Pollutant p, float ugm3);
  static int16_t mapAQI(Pollutant p, float ugm3_trunc);
};

} // namespace AQI
