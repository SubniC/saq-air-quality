#pragma once

// Jitter pseudo-aleatorio dentro de ±pct% (pct=20 => ±20%)
static inline unsigned long rnd_jitter(unsigned long base, unsigned pct) {
  // evita <random>; usa Time.now() y millis() para variar:
  unsigned r = (unsigned)((Time.now() ^ millis()) & 0xFFFF);
  long span = (long)base * (long)pct / 100L;
  long delta = (long)(r % (2*span + 1)) - (long)span; // [-span, +span]
  long v = (long)base + delta;
  return (v < 0) ? 0UL : (unsigned long)v;
}

static inline unsigned long next_backoff(unsigned long prev, unsigned long initial_backoff=2000UL, unsigned long max_backoff=60000UL, uint8_t backoff_multiplier=2, uint8_t backoff_jitter_percent=20) {
  // 0 -> 2s -> 4s -> 8s ... tope ~60s, con jitter ±20%
  unsigned long base = (prev == 0) ? initial_backoff : (prev * backoff_multiplier);
  if (base > max_backoff) base = max_backoff;
  if(backoff_jitter_percent > 0)
  {
    return rnd_jitter(base, backoff_jitter_percent);
  }
  return base;
}