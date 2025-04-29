// Filter.cpp
#include "Filter.h"
#include <cmath>

#define PI 3.14159265358979323846


Filter::Filter(FilterType type, float sample_rate, float freq1, float freq2, float q) {
    // Initialize delay lines to zero
    x1 = x2 = y1 = y2 = 0.0f;

    // Compute coefficients based on filter type
    switch (type) {
        case FilterType::HighPass: {
            float omega = 2.0f * PI * freq1 / sample_rate;
            float alpha = sin(omega) / (2.0f * q);

            a0 = 1.0f + alpha;
            a1 = -2.0f * cos(omega);
            a2 = 1.0f - alpha;
            b0 = (1.0f + cos(omega)) / 2.0f;
            b1 = -(1.0f + cos(omega));
            b2 = (1.0f + cos(omega)) / 2.0f;
            break;
        }
        case FilterType::BandPass: {
            float wc = 2.0f * PI * sqrt(freq1 * freq2) / sample_rate;
            float bw = 2.0f * PI * (freq2 - freq1) / sample_rate;

            float alpha = sin(bw) * sinh(log(2.0f) / 2.0f * 1.0f * PI / 2.0f);
            float cosw = cos(wc);

            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw;
            a2 = 1.0f - alpha;
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            break;
        }
        case FilterType::LowPass: {
            float omega = 2.0f * PI * freq1 / sample_rate;
            float alpha = sin(omega) / (2.0f * q);

            a0 = 1.0f + alpha;
            a1 = -2.0f * cos(omega);
            a2 = 1.0f - alpha;
            b0 = (1.0f - cos(omega)) / 2.0f;
            b1 = 1.0f - cos(omega);
            b2 = (1.0f - cos(omega)) / 2.0f;
            break;
        }
        default:
            a0 = 1.0f; a1 = a2 = b0 = b1 = b2 = 0.0f;
            break;
    }

    // Normalize coefficients by dividing by a0
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    a0 = 1.0f;
}

float Filter::process(float input) {
    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    // Update delay lines
    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

float rectify(float input) {
    return std::abs(input);
}