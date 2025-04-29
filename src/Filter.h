#ifndef FILTER_H
#define FILTER_H

// Enum to define filter types
enum class FilterType {
    HighPass,
    BandPass,
    LowPass
};

// Filter class
class Filter {
private:
    // Coefficients for the IIR difference equation
    float b0, b1, b2; // Feedforward coefficients
    float a0, a1, a2; // Feedback coefficients

    // Delay lines
    float x1, x2, y1, y2;

public:
    Filter(FilterType type, float sample_rate, float freq1, float freq2 = 0.0f, float q = 1.0f);

    // Process a single input sample and return the output
    float process(float input);
};

// Rectifier (absolute value)
float rectify(float input);

#endif // FILTER_H