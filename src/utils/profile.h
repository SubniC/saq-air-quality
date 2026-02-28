#pragma once

#include "config/config.h"

// Actívalo con: #define PROFILE_TIMES 1 (en tu config.h)
#ifdef PROFILE_TIMES

#include "utils/debug.h"

// Máximo de “nombres” distintos a perfilar
#ifndef PROF_MAX_ENTRIES
#define PROF_MAX_ENTRIES 24
#endif

namespace PROF {

struct Entry {
  const char* name = nullptr;
  uint32_t last_us = 0;
  uint32_t max_us  = 0;
  uint32_t sum_us  = 0;
  uint32_t count   = 0;
  uint32_t t_start = 0;
  bool     running = false;
};

// Acceso al array estático (sin global visible)
static inline Entry* entries()
{
  static Entry s_entries[PROF_MAX_ENTRIES] = {};
  return s_entries;
}

// Busca un slot por nombre; si no existe, reserva uno libre.
// Si no hay hueco, usa el último.
static inline Entry& slot_for(const char* name)
{
  Entry* arr = entries();

  // 1) buscar coincidencia exacta
  for (int i = 0; i < PROF_MAX_ENTRIES; ++i) {
    if (arr[i].name == name) {
      return arr[i];
    }
  }

  // 2) si no hubo match por puntero, intentar por contenido (por si pasan cadenas no literales)
  if (name) {
    for (int i = 0; i < PROF_MAX_ENTRIES; ++i) {
      if (arr[i].name && strcmp(arr[i].name, name) == 0) {
        return arr[i];
      }
    }
  }

  // 3) slot libre
  for (int i = 0; i < PROF_MAX_ENTRIES; ++i) {
    if (arr[i].name == nullptr) {
      arr[i].name = name;
      return arr[i];
    }
  }

  // 4) sin hueco: reciclar el último
  return arr[PROF_MAX_ENTRIES - 1];
}

static inline void begin(const char* name)
{
  Entry &e = slot_for(name);
  e.t_start = micros();
  e.running = true;
}

static inline void end(const char* name)
{
  Entry &e = slot_for(name);
  if (!e.running) return;

  const uint32_t dt = micros() - e.t_start;
  e.running = false;
  e.last_us = dt;
  if (dt > e.max_us) e.max_us = dt;
  e.sum_us += dt;
  e.count++;
}

// Vuelca por log y resetea acumulados
static inline void snapshot_and_reset()
{
  Entry* arr = entries();
  for (int i = 0; i < PROF_MAX_ENTRIES; ++i) {
    Entry &e = arr[i];
    if (!e.name) continue;

    const uint32_t avg = e.count ? (e.sum_us / e.count) : 0;
    // Usa tus macros de log; aquí asumo LOG_INFO ya existente
    LOG_INFO("[PROF] %s last=%luus avg=%luus max=%luus n=%lu",
             e.name,
             (unsigned long)e.last_us,
             (unsigned long)avg,
             (unsigned long)e.max_us,
             (unsigned long)e.count);

    // reset parciales
    e.last_us = 0;
    e.sum_us  = 0;
    e.count   = 0;
    e.max_us  = 0; // si prefieres conservar el pico, comenta esta línea
  }
}

// RAII para perfilar scopes
struct Scope {
  const char* name;
  explicit Scope(const char* n) : name(n) { begin(name); }
  ~Scope() { end(name); }
};

} // namespace PROF

// Macros cómodas
#define PROF_INIT()        do{}while(0)
#define PROF_SCOPE(name)   PROF::Scope _prof_scope_##__LINE__{name}
#define PROF_BEGIN(name)   PROF::begin(name)
#define PROF_END(name)     PROF::end(name)
#define PROF_SNAPSHOT()    PROF::snapshot_and_reset()

#else  // PROFILE_TIMES desactivado

#define PROF_INIT()        do{}while(0)
#define PROF_SCOPE(name)   do{}while(0)
#define PROF_BEGIN(name)   do{}while(0)
#define PROF_END(name)     do{}while(0)
#define PROF_SNAPSHOT()    do{}while(0)

#endif
