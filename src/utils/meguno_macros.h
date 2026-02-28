#pragma once
#include "config/config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ==============================
//  Puerto por defecto
// ==============================
#ifndef MEGUNO_PORT
  #ifdef DEBUG_SERIAL_PORT
    #define MEGUNO_PORT DEBUG_SERIAL_PORT
  #else
    // Sin puerto, desactivamos Meguno para no romper compilación
    #undef ENABLE_MEGUNO_DEBUG
  #endif
#endif

// Tamaños configurables (puedes ajustarlos en config.h si quieres)
#ifndef MEGUNO_FMT_BUFSZ
  #define MEGUNO_FMT_BUFSZ 256  // donde hacemos vsnprintf
#endif
#ifndef MEGUNO_SAN_BUFSZ
  #define MEGUNO_SAN_BUFSZ 256  // salida saneada final
#endif

#if defined(ENABLE_MEGUNO_DEBUG)

// ==============================
//  Sanitizadores
// ==============================
static inline void _ml_sanitize_into_(char* out, size_t outsz, const char* in) {
  if (!out || outsz == 0) return;
  if (!in) { out[0] = '\0'; return; }

  size_t w = 0;
  for (size_t i = 0; in[i] && w + 1 < outsz; ++i) {
    unsigned char c = static_cast<unsigned char>(in[i]);

    if (c == '{') c = '(';
    else if (c == '}') c = ')';

    if (c == '\r' || c == '\n' || c == '\t') continue;

    const bool printable = (c >= 32 && c <= 126);
    out[w++] = printable ? char(c) : '.';
  }
  out[w] = '\0';
}

// Conservamos la versión que devuelve buffer estático para usos simples
static inline const char* _ml_sanitize_(const char* in) {
  static char out[MEGUNO_SAN_BUFSZ];
  _ml_sanitize_into_(out, sizeof(out), in);
  return out;
}

// ==============================
//  Helpers con variádicos
// ==============================

// ---------- printf-like format checking (opcional, seguro) ----------
#ifndef ML_PRINTF_ATTR
  #if defined(__GNUC__) || defined(__clang__)
    // Usa índices 1-based: (level=1, fmt=2, ...=3)
    #define ML_PRINTF_ATTR(fmt_idx, var_idx) __attribute__((format(printf, fmt_idx, var_idx)))
  #else
    #define ML_PRINTF_ATTR(fmt_idx, var_idx)
  #endif
#endif
// -------------------------------------------------------------------


static inline void _ml_message_printf_(const char* level, const char* fmt, ...) ML_PRINTF_ATTR(2,3);
static inline void _ml_message_printf_(const char* level, const char* fmt, ...) {
  char raw[MEGUNO_FMT_BUFSZ];
  raw[0] = '\0';

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(raw, sizeof(raw), fmt ? fmt : "", ap);
  va_end(ap);

  char safe[MEGUNO_SAN_BUFSZ];
  _ml_sanitize_into_(safe, sizeof(safe), raw);

  const char* lvl  = level ? level : "INFO";
  MEGUNO_PORT.printlnf("{MESSAGE|DATA|%s %s}", lvl, safe);
}

static inline void _ml_timeplot_t_(const char* series, float value) {
  char s[MEGUNO_SAN_BUFSZ];
  _ml_sanitize_into_(s, sizeof(s), series ? series : "series");
  MEGUNO_PORT.printlnf("{TIMEPLOT|DATA|%s|T|%.3f}", s, (double)value);
}

static inline void _ml_timeplot_tf_(const char* series, const char* fmt, ...) ML_PRINTF_ATTR(2,3);
static inline void _ml_timeplot_tf_(const char* series, const char* fmt, ...) {
  char raw[MEGUNO_FMT_BUFSZ];
  raw[0] = '\0';

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(raw, sizeof(raw), fmt ? fmt : "", ap);
  va_end(ap);

  char s[MEGUNO_SAN_BUFSZ];
  char v[MEGUNO_SAN_BUFSZ];
  _ml_sanitize_into_(s, sizeof(s), series ? series : "series");
  _ml_sanitize_into_(v, sizeof(v), raw);

  // Nota: 'v' ya contiene el número formateado (p.ej. "26.13")
  MEGUNO_PORT.printlnf("{TIMEPLOT|DATA|%s|T|%s}", s, v);
}

// ==============================
//  API pública (macros)
// ==============================
  #define ML_MESSAGE(level, fmt, ...)      do { _ml_message_printf_((level), (fmt), ##__VA_ARGS__); } while (0)

  #ifdef ENABLE_MEGUNO_TIMEPLOT_DEBUG
    #define ML_TIMEPLOT_T(series, value)   do { _ml_timeplot_t_((series), (float)(value)); } while (0)
    #define ML_TIMEPLOT_TF(series, fmt, ...) do { _ml_timeplot_tf_((series), (fmt), ##__VA_ARGS__); } while (0)
  #else
    #define ML_TIMEPLOT_T(series, value)     do {} while (0)
    #define ML_TIMEPLOT_TF(series, fmt, ...) do {} while (0)
  #endif

#else  // !ENABLE_MEGUNO_DEBUG

  #define ML_MESSAGE(level, fmt, ...)        do {} while (0)
  #define ML_TIMEPLOT_T(series, value)       do {} while (0)
  #define ML_TIMEPLOT_TF(series, fmt, ...)   do {} while (0)

#endif  // ENABLE_MEGUNO_DEBUG

#undef ML_PRINTF_ATTR