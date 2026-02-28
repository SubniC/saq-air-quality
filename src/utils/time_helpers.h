#pragma once

static inline uint32_t now_ms() 
{ 
    return millis(); 
}

// Pequeño helper de “budget” opcional (por ahora sin profiler):
static inline bool elapsed_since(uint32_t last, uint32_t period) {
  const uint32_t t = now_ms();
  return (uint32_t)(t - last) >= period;
}