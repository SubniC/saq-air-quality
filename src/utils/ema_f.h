#pragma once
#include <math.h>

// Lightweight exponential moving average using float.
// Drop-in replacement for RunningAverage when only addValue + getFastAverage
// are needed.  Uses 8 bytes (2 floats) instead of N*8 bytes (double array).
struct EmaF {
    float value = NAN;
    float alpha;

    explicit EmaF(uint8_t window = 1)
        : alpha(2.0f / (window + 1)) {}

    void add(float v) {
        value = isnan(value) ? v : (1.0f - alpha) * value + alpha * v;
    }

    float get() const { return value; }

    void clear() { value = NAN; }
};
