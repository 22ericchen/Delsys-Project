#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>

// Define M_PI for platforms where it's not defined
#define M_PI 3.14159265358979323846

// Constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 900;

// Synthetic EMG signal parameters
const float SAMPLE_RATE = 2000.0f; // Hz
const float TIME_STEP = 1.0f / SAMPLE_RATE;
const int BUFFER_SIZE = 1000; // Number of samples to display
const float EMG_FREQ = 20.0f; // Base EMG frequency (Hz)
const float NOISE_AMPLITUDE = 0.2f;
const float POWER_LINE_FREQ = 10.0f; 
const float POWER_LINE_AMPLITUDE = 0.3f;

// Filter cutoff frequencies
const float HIGHPASS_CUTOFF = 5.0f; 
const float BANDPASS_LOW = 5.0f;
const float BANDPASS_HIGH = 50.0f; 
const float LOWPASS_CUTOFF = 2.0f;

// Circular buffers for raw, filtered, and envelope signals
std::vector<float> raw_signal(BUFFER_SIZE, 0.0f);
std::vector<float> filtered_signal(BUFFER_SIZE, 0.0f);
std::vector<float> envelope_signal(BUFFER_SIZE, 0.0f);
int buffer_index = 0;

// Random number generator for noise and EMG variation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(-NOISE_AMPLITUDE, NOISE_AMPLITUDE);
std::uniform_real_distribution<float> amp_dist(0.8f, 1.2f);
std::uniform_real_distribution<float> freq_dist(0.9f, 1.1f);
std::uniform_real_distribution<float> burst_dist(0.0f, 1.0f);
std::uniform_real_distribution<float> burst_scale(1.0f, 3.0f);

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
    Filter(FilterType type, float sample_rate, float freq1, float freq2 = 0.0f, float q = 1.0f) {
        // Initialize delay lines to zero
        x1 = x2 = y1 = y2 = 0.0f;

        // Compute coefficients based on filter type
        switch (type) {
            case FilterType::HighPass: {
                float omega = 2.0f * M_PI * freq1 / sample_rate;
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
                float wc = 2.0f * M_PI * sqrt(freq1 * freq2) / sample_rate;
                float bw = 2.0f * M_PI * (freq2 - freq1) / sample_rate;

                float alpha = sin(bw) * sinh(log(2.0f) / 2.0f * 1.0f * M_PI / 2.0f);
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
                float omega = 2.0f * M_PI * freq1 / sample_rate;
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

    // Process a single input sample and return the output
    float process(float input) {
        float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        // Update delay lines
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;

        return output;
    }
};

// Function to check OpenGL errors
void check_gl_error() {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error: " << error << std::endl;
    }
}

// Generate synthetic EMG signal with randomness
float generate_emg_signal(float t) {
    static float phase1 = 0.0f;
    static float phase2 = 0.0f;
    static float burst_factor = 1.0f;
    static int burst_duration = 0;

    if (buffer_index % 50 == 0) {
        if (burst_dist(gen) < 0.2f) {
            burst_factor = burst_scale(gen);
            burst_duration = 100;
        } else if (burst_duration <= 0) {
            burst_factor = 1.0f;
        }
    }
    if (burst_duration > 0) {
        burst_duration--;
    }

    float amp1 = 0.5f * amp_dist(gen);
    float amp2 = 0.3f * amp_dist(gen);
    float freq1 = EMG_FREQ * freq_dist(gen);
    float freq2 = (EMG_FREQ * 1.5f) * freq_dist(gen);

    phase1 += 2.0f * M_PI * freq1 * TIME_STEP;
    phase2 += 2.0f * M_PI * freq2 * TIME_STEP;

    float emg = burst_factor * (amp1 * sin(phase1) + amp2 * sin(phase2));
    emg += POWER_LINE_AMPLITUDE * sin(2.0f * M_PI * POWER_LINE_FREQ * t);
    emg += dist(gen);

    return emg;
}

// Rectifier (absolute value)
float rectify(float input) {
    return std::abs(input);
}

// Normalize signal for visualization
float normalize_for_display(float value, float max_amplitude) {
    const float display_range = 0.5f; // Amplitude range to prevent overlap
    if (max_amplitude == 0.0f) return value;
    return (value / max_amplitude) * display_range;
}

// Render signals using OpenGL
void render_signals() {
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error();

    float max_raw = 0.0f;
    float max_filtered = 0.0f;
    float max_envelope = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        max_raw = std::max(max_raw, std::abs(raw_signal[i]));
        max_filtered = std::max(max_filtered, std::abs(filtered_signal[i]));
        max_envelope = std::max(max_envelope, envelope_signal[i]);
    }

    // Draw raw signal (top, centered at y = 0.75)
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    check_gl_error();
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = 0.75f + normalize_for_display(raw_signal[(buffer_index + i) % BUFFER_SIZE], max_raw);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    // Draw zero line for raw signal (at y = 0.75)
    glColor3f(0.5f, 0.5f, 0.5f); // Gray
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.75f);
    glVertex2f(1.0f, 0.75f);
    glEnd();

    // Draw filtered signal (middle, centered at y = 0.0)
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = 0.0f + normalize_for_display(filtered_signal[(buffer_index + i) % BUFFER_SIZE], max_filtered);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    // Draw zero line for filtered signal (at y = 0.0)
    glColor3f(0.5f, 0.5f, 0.5f); // Gray
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glEnd();

    // Draw envelope signal (bottom, centered at y = -0.75)
    glColor3f(0.0f, 0.0f, 1.0f); // Blue
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = -0.75f + normalize_for_display(envelope_signal[(buffer_index + i) % BUFFER_SIZE], max_envelope);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    // Draw zero line for envelope signal (at y = -0.75)
    glColor3f(0.5f, 0.5f, 0.5f); // Gray
    glBegin(GL_LINES);
    glVertex2f(-1.0f, -0.75f);
    glVertex2f(1.0f, -0.75f);
    glEnd();

    // Draw separator lines
    glColor3f(1.0f, 1.0f, 1.0f); // White
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f); // Separator between raw and filtered
    glVertex2f(1.0f, 0.0f);
    glVertex2f(-1.0f, -0.75f); // Separator between filtered and envelope
    glVertex2f(1.0f, -0.75f);
    glEnd();
    check_gl_error();
}

int main() {
    std::cout << "Starting program..." << std::endl;
    std::cout << "Red (Top): Initial Signal (Raw EMG)" << std::endl;
    std::cout << "Green (Middle): Filtered Signal" << std::endl;
    std::cout << "Blue (Bottom): Envelope Signal (Rectified + Smoothed)" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    std::cout << "GLFW initialized successfully" << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "EMG Signal Filtering", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "GLFW window created successfully" << std::endl;

    glfwMakeContextCurrent(window);

    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        std::cout << "OpenGL Version: " << version << std::endl;
    } else {
        std::cerr << "Failed to get OpenGL version" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    std::cout << "Renderer: " << (renderer ? reinterpret_cast<const char*>(renderer) : "Unknown") << std::endl;
    std::cout << "Vendor: " << (vendor ? reinterpret_cast<const char*>(vendor) : "Unknown") << std::endl;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    check_gl_error();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.5, 1.5, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    check_gl_error();

    std::cout << "OpenGL setup complete, entering main loop..." << std::endl;

    // Create filter instances
    Filter highPassFilter(FilterType::HighPass, SAMPLE_RATE, HIGHPASS_CUTOFF);
    Filter bandPassFilter(FilterType::BandPass, SAMPLE_RATE, BANDPASS_LOW, BANDPASS_HIGH);
    Filter lowPassFilter(FilterType::LowPass, SAMPLE_RATE, LOWPASS_CUTOFF);

    float t = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // Processing chain
        float raw = generate_emg_signal(t);
        float highpassed = highPassFilter.process(raw);
        float bandpass_filtered = bandPassFilter.process(highpassed);
        float rectified = rectify(bandpass_filtered);
        float enveloped = lowPassFilter.process(rectified);
        
        raw_signal[buffer_index] = raw;
        filtered_signal[buffer_index] = bandpass_filtered;
        envelope_signal[buffer_index] = enveloped;
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;

        t += TIME_STEP;

        if (buffer_index % 100 == 0) {
            float max_raw = *std::max_element(raw_signal.begin(), raw_signal.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
            float max_filtered = *std::max_element(filtered_signal.begin(), filtered_signal.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
            float max_envelope = *std::max_element(envelope_signal.begin(), envelope_signal.end());
            std::cout << "Raw: " << raw << ", High-passed: " << highpassed << ", Band-passed: " << bandpass_filtered 
                      << ", Rectified: " << rectified << ", Enveloped: " << enveloped << std::endl;
            std::cout << "Max Raw Amplitude: " << max_raw << ", Max Filtered Amplitude: " << max_filtered 
                      << ", Max Envelope: " << max_envelope << std::endl;
        }

        render_signals();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    std::cout << "Cleaning up..." << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Program exited successfully" << std::endl;
    return 0;
}