#pragma once
#include "application.h"   // Particle DeviceOS: millis(), Serial.printlnf(), etc.
#include "config/config.h"

// ============================== Configurables ==============================

// Puerto serie a usar para log (puedes sobreescribirlo desde config.h)
#ifndef DEBUG_SERIAL_PORT
  #define DEBUG_SERIAL_PORT Serial
#endif

// Baud solo si decides inicializar desde aquí
#ifndef DEBUG_BAUD
  #define DEBUG_BAUD 115200
#endif

// ============================== Init opcional ==============================
#ifdef HAS_ANY_SERIAL_OUTPUT_ENABLED
  #define LOG_INIT() do { \
    DEBUG_SERIAL_PORT.begin(DEBUG_BAUD); \
    waitFor(Serial.isConnected, 3000); \
  } while(0)
#else
  #define LOG_INIT() do{}while(0)
#endif

// ============================== Núcleo print ===============================
#define _LOG_LNF(fmt, ...) DEBUG_SERIAL_PORT.printlnf(fmt, ##__VA_ARGS__)

// ============================== Canales ====================================
// Habilita/inhabilita cada canal con tus flags globales:
//   - ENABLE_SERIAL_DEBUG
//   - ENABLE_SERIAL_MQTT_DEBUG

#ifdef ENABLE_SERIAL_DEBUG
  #define DBG(fmt, ...)          _LOG_LNF(fmt, ##__VA_ARGS__)
  #define DBG_TAG(tag, fmt, ...) _LOG_LNF("[%s] " fmt, tag, ##__VA_ARGS__)
#else
  #define DBG(...)               do{}while(0)
  #define DBG_TAG(...)           do{}while(0)
#endif

#ifdef ENABLE_SERIAL_MQTT_DEBUG
  #define MQTT_LOG(fmt, ...)          _LOG_LNF(fmt, ##__VA_ARGS__)
  #define MQTT_LOG_TAG(tag, fmt, ...) _LOG_LNF("[%s] " fmt, tag, ##__VA_ARGS__)
#else
  #define MQTT_LOG(...)               do{}while(0)
  #define MQTT_LOG_TAG(...)           do{}while(0)
#endif

// ============================== Utilidades =================================

// Imprime solo una vez (por *punto de uso*)
#define LOG_ONCE(channel_macro, fmt, ...) do { \
  static bool _once_##__COUNTER__ = false;      \
  if (!_once_##__COUNTER__) {                   \
    _once_##__COUNTER__ = true;                 \
    channel_macro(fmt, ##__VA_ARGS__);          \
  }                                             \
} while(0)

// Imprime cada N ms (rate limiting por *punto de uso*)
#define LOG_EVERY_MS(channel_macro, interval_ms, fmt, ...) do { \
  static uint32_t _last_##__COUNTER__ = 0;                       \
  uint32_t _now = millis();                                      \
  if (_now - _last_##__COUNTER__ >= (uint32_t)(interval_ms)) {   \
    _last_##__COUNTER__ = _now;                                  \
    channel_macro(fmt, ##__VA_ARGS__);                           \
  }                                                              \
} while(0)

// Hexdump sencillo (16 bytes/linea) — solo genera código si el canal está activo
#ifdef ENABLE_SERIAL_DEBUG
  static inline void DBG_HEXDUMP(const void* vp, size_t n) {
    const uint8_t* p = (const uint8_t*)vp;
    char line[96];
    for (size_t i = 0; i < n; i += 16) {
      size_t o = 0;
      o += snprintf(line+o, sizeof(line)-o, "%04u: ", (unsigned)i);
      for (size_t j = 0; j < 16 && (i+j) < n; ++j)
        o += snprintf(line+o, sizeof(line)-o, "%02X ", p[i+j]);
      _LOG_LNF("%s", line);
    }
  }
#else
  static inline void DBG_HEXDUMP(const void*, size_t) {}
#endif

// Temporizador de ámbito (profiling) — activar con #define PERF_TIMERS 1
#ifndef PERF_TIMERS
  #define PERF_TIMERS 0
#endif

#if PERF_TIMERS && defined(ENABLE_SERIAL_DEBUG)
  struct _DbgScopeTimer {
    const char* name; uint32_t t0;
    explicit _DbgScopeTimer(const char* n) : name(n), t0(millis()) {}
    ~_DbgScopeTimer() {
      DBG("[T] %s took %lums", name, (unsigned long)(millis() - t0));
    }
  };
  #define SCOPE_TIMER(name) _DbgScopeTimer _sc_##__COUNTER__(name)
#else
  #define SCOPE_TIMER(name) do{}while(0)
#endif
