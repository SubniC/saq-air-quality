#include <math.h>
#include "utils/debug.h"

static inline float apply_offset(float v, float offs, float minv, float maxv, bool clamp_min0=true) {
  if (offs == 0.0f) return v;
  float r = v + offs;
  if (clamp_min0 && r < 0) r = 0;
  if (!isnan(minv)) r = (r < minv) ? minv : r;
  if (!isnan(maxv)) r = (r > maxv) ? maxv : r;
  LOG_DBG("{FUSED} Applied offset %.2f to value %.2f with result %.2f", offs, v, r);
  return r;
}