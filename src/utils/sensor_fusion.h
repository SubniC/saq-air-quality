#include <math.h>

namespace FUSE
{

  struct SensorMeasurement
  {
    float v;    // valor
    float w;    // peso (>0 => contribuye)
  };

  // Requisito mínimo de validez numérica (ajusta a tu helper si ya lo tienes):
  static inline bool sensor_value_ok(float x)
  {
    return !isnan(x) && isfinite(x);
  }

  // Media ponderada: devuelve true si *out se escribe con un valor válido.
  // contributors/wsum_out son opcionales (útiles para debug).
  static inline bool weighted_mean(const SensorMeasurement *s, size_t n, float *out)
  {
    if (!out || !s || n == 0)
      return false;

    // Caso 1 fuente (early-exit benigno)
    if (n == 1)
    {
      const bool ok1 = (sensor_value_ok(s[0].v) && s[0].w > 0.f);
      if (!ok1)
        return false;
      *out = s[0].v;
      return true;
    }

    float sum = 0.f, wsum = 0.f;
    uint8_t cnt = 0;

    for (size_t i = 0; i < n; ++i)
    {
      const auto &si = s[i];
      if (!sensor_value_ok(si.v) || si.w <= 0.f)
        continue;
      sum += si.v * si.w;
      wsum += si.w;
      ++cnt;
    }

    if (wsum <= 0.f)
      return false;

    *out = sum / wsum;
    return true;
  }

  // Atajo para 2 fuentes (ahorra crear arrays en call-site)
  static inline bool weighted_mean2(const SensorMeasurement &a, const SensorMeasurement &b, float *out)
  {
    SensorMeasurement tmp[2] = {a, b};
    return weighted_mean(tmp, 2, out);
  }

} // namespace FUSE
