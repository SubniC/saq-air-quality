#pragma once

#include "utils/debug_macros.h"
#include "utils/meguno_macros.h"

// ===== Aggregators: una sola llamada → dos salidas (DBG + MegunoLink) =====

// Mensaje genérico con canal/etiqueta (p.ej. "INFO", "ERROR", "DATA", "NET"...)
#define LOG_MSG(CHANNEL, FMT, ...)                                            \
  do {                                                                        \
    DBG_TAG((CHANNEL), FMT, ##__VA_ARGS__);                                    \
    ML_MESSAGE((CHANNEL), FMT, ##__VA_ARGS__);                                 \
  } while (0)

// Azúcar sintáctico
#define LOG_DBG(FMT, ...)   LOG_MSG("DEBUG",  FMT, ##__VA_ARGS__)
#define LOG_INFO(FMT, ...)   LOG_MSG("INFO",  FMT, ##__VA_ARGS__)
#define LOG_WARN(FMT, ...)   LOG_MSG("WARN",  FMT, ##__VA_ARGS__)
#define LOG_ERROR(FMT, ...)  LOG_MSG("ERROR", FMT, ##__VA_ARGS__)

// Variante para timeplot: log humano + trazado en MegunoLink
#define LOG_TIMEPLOT(PLOT, SERIES, VALUE, HUMAN_FMT, ...)                     \
  do {                                                                        \
    DBG_TAG("TP", HUMAN_FMT, ##__VA_ARGS__);                                  \
    ML_TIMEPLOT_T((SERIES), (VALUE));                                    \
  } while (0)

// Helper para loggear y devolver en una línea
#define LOG_RETURN(ERRVAL, CHANNEL, FMT, ...)                                 \
  do {                                                                        \
    LOG_MSG((CHANNEL), FMT, ##__VA_ARGS__);                                    \
    return (ERRVAL);                                                           \
  } while (0)

#if defined(ENABLE_SERIAL_MQTT_DEBUG) || defined(ENABLE_MEGUNO_MQTT_DEBUG)
  #define LOG_MQTT(TOPIC, PAYLOAD, DIRECTION)                                        \
    do {                                                                             \
      const char* _t_ = (TOPIC) ? (TOPIC) : "(null)";                                \
      const char* _p_ = (PAYLOAD) ? (PAYLOAD) : "(null)";                            \
      const char* _d_ = (DIRECTION) ? (DIRECTION) : "OUT";                           \
      MQTT_LOG_TAG("MQTT", "%s topic=[%s] payload=[%s]", _d_, _t_, _p_);             \
      ML_MESSAGE("MQTT", "%s TOPIC[%s] MESSAGE[%s] T[%ld]",                          \
                 _d_, _t_, _p_, (long)Time.local());                                 \
    } while (0)
#else
  #define LOG_MQTT(TOPIC, PAYLOAD, DIRECTION) do{}while(0)
#endif

#define LOG_MQTT_PUBLISH(TOPIC, PAYLOAD) LOG_MQTT((TOPIC), (PAYLOAD), "PUBLISH")
#define LOG_MQTT_RECEIVE(TOPIC, PAYLOAD)  LOG_MQTT((TOPIC), (PAYLOAD), "RECEIVE")