#ifndef IIRFILTER_H
#define IIRFILTER_H

#include <Arduino.h>

struct BiquadIIR {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
    bool initialized;

    BiquadIIR(float b0_, float b1_, float b2_, float a1_, float a2_)
        : b0(b0_), b1(b1_), b2(b2_), a1(a1_), a2(a2_), x1(0.0f), x2(0.0f), y1(0.0f), y2(0.0f), initialized(false) {}

    void reset(float value) {
        x1 = x2 = value;
        y1 = y2 = value;
        initialized = true;
    }

    float process(float x) {
        if (!initialized) {
            reset(x);
            return x;
        }

        float y = (b0 * x) + (b1 * x1) + (b2 * x2) - (a1 * y1) - (a2 * y2);

        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;

        return y;
    }
};

#endif // IIRFILTER_H
