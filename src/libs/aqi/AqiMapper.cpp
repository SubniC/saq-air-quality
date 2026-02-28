#include "AqiMapper.h"
#include <math.h>

namespace AQI {

struct Breakpoint { float Cl; float Ch; int Il; int Ih; };


// Breakpoints EPA actuales (PM2.5 en µg/m³ con 0.1; PM10 entero µg/m³).
// PM2.5: [0.0–12.0]→0–50; [12.1–35.4]→51–100; [35.5–55.4]→101–150; etc.
static const Breakpoint BP_PM25[] = {
  {  0.0f,  12.0f,   0,  50 },
  { 12.1f,  35.4f,  51, 100 },
  { 35.5f,  55.4f, 101, 150 },
  { 55.5f, 150.4f, 151, 200 },
  {150.5f, 250.4f, 201, 300 },
  {250.5f, 350.4f, 301, 400 },
  {350.5f, 500.4f, 401, 500 },
};

static const Breakpoint BP_PM10[] = {
  {   0.0f,   54.0f,   0,  50 },
  {  55.0f,  154.0f,  51, 100 },
  { 155.0f,  254.0f, 101, 150 },
  { 255.0f,  354.0f, 151, 200 },
  { 355.0f,  424.0f, 201, 300 },
  { 425.0f,  504.0f, 301, 400 },
  { 505.0f,  604.0f, 401, 500 },
};


static inline int16_t clampAQI(float aqi) {
  if (!isfinite(aqi)) return -1;
  if (aqi < 0) aqi = 0;
  if (aqi > 500) aqi = 500;
  return (int16_t)lroundf(aqi);
}

// Implementación genérica para cualquier tabla de breakpoints.
// Deducimos N en tiempo de compilación y evitamos errores de tipo.
template <size_t N>
static int16_t piecewise(const float C, const Breakpoint (&bp)[N]) {
  if (!isfinite(C)) return -1;
  for (size_t i = 0; i < N; ++i) {
    if (C >= bp[i].Cl && C <= bp[i].Ch) {
      const float aqi = ((bp[i].Ih - bp[i].Il) / (bp[i].Ch - bp[i].Cl)) * (C - bp[i].Cl) + bp[i].Il;
      return clampAQI(aqi);
    }
  }
  // Extrapola último tramo si supera el máximo de tabla
  const Breakpoint& last = bp[N-1];
  if (C > last.Ch) {
    const float aqi = ((last.Ih - last.Il) / (last.Ch - last.Cl)) * (C - last.Cl) + last.Il;
    return clampAQI(aqi);
  }
  return -1;
}

int16_t AqiMapper::aqiForPM25(float conc_ugm3_truncated_0p1) {
  return piecewise(conc_ugm3_truncated_0p1, BP_PM25);
}

int16_t AqiMapper::aqiForPM10(float conc_ugm3_truncated_int) {
  return piecewise(conc_ugm3_truncated_int, BP_PM10);
}

} // namespace AQI
