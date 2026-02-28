#pragma once
#include "application.h"
#include <math.h>
// Mapeo EPA de concentración -> AQI (PM2.5 y PM10) con truncado previo.
// Reglas:
//  - PM2.5: truncar a 0.1 µg/m³ (hacia abajo)
//  - PM10:  truncar a entero µg/m³ (hacia abajo)
//  - Luego localizar tramo (breakpoints) y linealizar.

namespace AQI {

enum class Pollutant : uint8_t { PM25 = 0, PM10 = 1 };

struct AqiPoint {
  float conc;    // concentración (µg/m³), tras truncado EPA
  int16_t aqi;   // AQI calculado [0..500]
};

class AqiMapper {
 public:
  static inline float truncatePM25(float ugm3) {
    if (!isfinite(ugm3) || ugm3 < 0) return NAN;
    return floorf(ugm3 * 10.0f) / 10.0f;  // 0.1
  }
  static inline float truncatePM10(float ugm3) {
    if (!isfinite(ugm3) || ugm3 < 0) return NAN;
    return floorf(ugm3);                   // 1.0
  }

  static int16_t aqiForPM25(float conc_ugm3_truncated_0p1);
  static int16_t aqiForPM10(float conc_ugm3_truncated_int);

  static inline int16_t aqi(Pollutant p, float conc_ugm3) {
    if (!isfinite(conc_ugm3) || conc_ugm3 < 0) return -1;
    if (p == Pollutant::PM25) return aqiForPM25(truncatePM25(conc_ugm3));
    else                      return aqiForPM10(truncatePM10(conc_ugm3));
  }
};

} // namespace AQI
