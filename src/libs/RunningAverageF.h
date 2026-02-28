#pragma once

#include <stdlib.h>
#include <math.h>

// Lightweight float-based RunningAverage.
// Saves ~50% RAM vs the original double-based RunningAverage.
// Only includes the methods actually used by ClapDetector.
class RunningAverageF {
public:
    explicit RunningAverageF(uint8_t size)
        : _size(size)
    {
        _ar = (float*)malloc(_size * sizeof(float));
        if (!_ar) _size = 0;
        clear();
    }

    ~RunningAverageF() { free(_ar); }

    void clear() {
        _cnt = 0;
        _idx = 0;
        _sum = 0.0f;
        for (uint8_t i = 0; i < _size; i++) _ar[i] = 0.0f;
    }

    void addValue(float value) {
        if (!_ar) return;
        _sum -= _ar[_idx];
        _ar[_idx] = value;
        _sum += _ar[_idx];
        if (++_idx == _size) _idx = 0;
        if (_cnt < _size) _cnt++;
    }

    float getFastAverage() const {
        if (_cnt == 0) return NAN;
        return _sum / _cnt;
    }

    float GetStandardDeviation() const {
        if (_cnt < 2) return 0.0f;
        float avg = _sum / _cnt;
        float acc = 0.0f;
        for (uint8_t i = 0; i < _cnt; i++) {
            float d = _ar[i] - avg;
            acc += d * d;
        }
        return sqrtf(acc / (_cnt - 1));
    }

private:
    uint8_t _size;
    uint8_t _cnt = 0;
    uint8_t _idx = 0;
    float   _sum = 0.0f;
    float*  _ar  = nullptr;
};
