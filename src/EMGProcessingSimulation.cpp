#include <GLFW/glfw3.h>
#include <vector>
#include <random>
#include <iostream>
#include <algorithm>
#include <chrono>
#include "Filter.h"

#define PI 3.14159265358979323846

// Window dimensions for visualization
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 900;

// Synthetic EMG signal parameters
const float SAMPLE_RATE = 2000.0f; // Hz, sampling rate for the simulation
const float TIME_STEP = 1.0f / SAMPLE_RATE; // Time between samples (seconds)
const int BUFFER_SIZE = 1000; // Number of samples to display (0.5 seconds at 2000 Hz)
const float EMG_FREQ = 20.0f; // Base EMG frequency (Hz)
const float NOISE_AMPLITUDE = 0.2f; // Amplitude of random noise
const float POWER_LINE_FREQ = 3.0f; // Power line interference frequency (Hz)
const float POWER_LINE_AMPLITUDE = 1.0f; // Amplitude of power line interference

// Filter parameters
float adjustable_highpass_cutoff = 5.0f; // High-pass cutoff frequency (Hz), adjustable
const float BANDPASS_LOW = 5.0f; // Band-pass low cutoff frequency (Hz)
const float BANDPASS_HIGH = 50.0f; // Band-pass high cutoff frequency (Hz)
const float LOWPASS_CUTOFF = 2.0f; // Low-pass cutoff for envelope (Hz)

// Circular buffers to store the most recent samples for visualization
std::vector<float> raw_signal(BUFFER_SIZE, 0.0f); // Raw EMG signal
std::vector<float> filtered_signal(BUFFER_SIZE, 0.0f); // Filtered EMG signal (after high-pass and band-pass)
std::vector<float> envelope_signal(BUFFER_SIZE, 0.0f); // Envelope signal (rectified and smoothed)
int buffer_index = 0; // Current index in the circular buffers

// Random number generator for introducing variability in the EMG signal
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(-NOISE_AMPLITUDE, NOISE_AMPLITUDE); // Noise distribution
std::uniform_real_distribution<float> amp_dist(0.8f, 1.2f); // Amplitude variation (0.8x to 1.2x)
std::uniform_real_distribution<float> freq_dist(0.9f, 1.1f); // Frequency variation (0.9x to 1.1x)
std::uniform_real_distribution<float> burst_dist(0.0f, 1.0f); // Probability of burst (0 to 1)
std::uniform_real_distribution<float> burst_scale(1.0f, 3.0f); // Burst amplitude scaling (1x to 3x)

// State for pause/resume functionality
bool is_paused = false;

// Callback for key presses to toggle pause/resume and adjust high-pass filter
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
        is_paused = !is_paused;
        std::cout << (is_paused ? "Simulation Paused" : "Simulation Resumed") << std::endl;
    }
    // Adjust high-pass filter cutoff with arrow keys
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_UP) {
            adjustable_highpass_cutoff += 0.5f;
            std::cout << "High-pass cutoff increased to: " << adjustable_highpass_cutoff << " Hz" << std::endl;
        }
        if (key == GLFW_KEY_DOWN) {
            adjustable_highpass_cutoff = std::max(1.0f, adjustable_highpass_cutoff - 0.5f); // Ensure cutoff doesn't go below 1 Hz
            std::cout << "High-pass cutoff decreased to: " << adjustable_highpass_cutoff << " Hz" << std::endl;
        }
    }
}

// Check for OpenGL errors after a specific operation
void check_gl_error(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after " << operation << ": " << error << std::endl;
    }
}

// Generate a synthetic EMG signal with random variations
// Parameters:
// - t: Current time (seconds)
// Returns: EMG signal value at time t
float generate_emg_signal(float t) {
    static float phase1 = 0.0f; // Phase of first EMG component
    static float phase2 = 0.0f; // Phase of second EMG component
    static float burst_factor = 1.0f; // Scaling factor for muscle bursts
    static int burst_duration = 0; // Duration of current burst (samples)

    if (buffer_index % 50 == 0) {
        if (burst_dist(gen) < 0.2f) { // 20% chance of a burst
            burst_factor = burst_scale(gen); // Scale amplitude by 1x to 3x
            burst_duration = 100; // Burst lasts 50 ms (100 samples)
        } else if (burst_duration <= 0) {
            burst_factor = 1.0f; // Reset to no scaling
        }
    }
    if (burst_duration > 0) {
        burst_duration--;
    }

    // Generate two EMG components with random amplitude and frequency variations
    float amp1 = 0.5f * amp_dist(gen); // Amplitude of first component (0.4 to 0.6)
    float amp2 = 0.3f * amp_dist(gen); // Amplitude of second component (0.24 to 0.36)
    float freq1 = EMG_FREQ * freq_dist(gen); // Frequency of first component (18 to 22 Hz)
    float freq2 = (EMG_FREQ * 1.5f) * freq_dist(gen); // Frequency of second component (27 to 33 Hz)

    // Update phases for continuous sine waves
    phase1 += 2.0f * PI * freq1 * TIME_STEP;
    phase2 += 2.0f * PI * freq2 * TIME_STEP;

    // Combine EMG components, apply burst scaling, and add interference and noise
    float emg = burst_factor * (amp1 * sin(phase1) + amp2 * sin(phase2));
    emg += POWER_LINE_AMPLITUDE * sin(2.0f * PI * POWER_LINE_FREQ * t);
    emg += dist(gen);

    // Check for numerical stability
    if (std::isnan(emg) || std::isinf(emg)) {
        std::cerr << "Warning: Generated EMG signal is NaN or Inf at t = " << t << std::endl;
        return 0.0f; // Return a safe value to prevent crashes
    }

    return emg;
}

// Normalize a signal value for visualization
// Parameters:
// - value: Signal value to normalize
// - max_amplitude: Maximum amplitude for scaling
// Returns: Normalized value in the range [-display_range, display_range]
float normalize_for_display(float value, float max_amplitude) {
    const float display_range = 0.5f; // Amplitude range to prevent overlap
    if (max_amplitude == 0.0f) {
        std::cerr << "Warning: Max amplitude is zero, normalization skipped" << std::endl;
        return value;
    }
    return (value / max_amplitude) * display_range;
}

// Render the raw, filtered, and envelope signals using OpenGL
void render_signals() {
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error("glClear");

    // Calculate maximum amplitudes for normalization
    float max_raw = 0.0f;
    float max_filtered = 0.0f;
    float max_envelope = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        max_raw = std::max(max_raw, std::abs(raw_signal[i]));
        max_filtered = std::max(max_filtered, std::abs(filtered_signal[i]));
        max_envelope = std::max(max_envelope, envelope_signal[i]);
    }

    // Render raw EMG signal (top, red)
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    check_gl_error("glColor3f (raw)");
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = 0.75f + normalize_for_display(raw_signal[(buffer_index + i) % BUFFER_SIZE], max_raw);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error("glEnd (raw)");

    // Draw zero line for raw EMG signal
    glColor3f(0.5f, 0.5f, 0.5f); // Gray
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.75f);
    glVertex2f(1.0f, 0.75f);
    glEnd();

    // Render filtered signal (middle, green)
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    check_gl_error("glColor3f (filtered)");
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = 0.0f + normalize_for_display(filtered_signal[(buffer_index + i) % BUFFER_SIZE], max_filtered);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error("glEnd (filtered)");

    // Draw zero line for filtered signal
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glEnd();

    // Render envelope signal (bottom, blue)
    glColor3f(0.0f, 0.0f, 1.0f); // Blue
    check_gl_error("glColor3f (envelope)");
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = -0.75f + normalize_for_display(envelope_signal[(buffer_index + i) % BUFFER_SIZE], max_envelope);
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error("glEnd (envelope)");

    // Draw zero line for envelope signal
    glColor3f(0.5f, 0.5f, 0.5f); // Gray
    glBegin(GL_LINES);
    glVertex2f(-1.0f, -0.75f);
    glVertex2f(1.0f, -0.75f);
    glEnd();
}

int main() {
    std::cout << "Starting program..." << std::endl;
    std::cout << "Red (Top): Initial Signal (Raw EMG)" << std::endl;
    std::cout << "Green (Middle): Filtered Signal" << std::endl;
    std::cout << "Blue (Bottom): Envelope Signal (Rectified + Smoothed)" << std::endl;
    std::cout << "Press SPACE to pause/resume the simulation" << std::endl;
    std::cout << "Press UP/DOWN to adjust high-pass filter cutoff (current: " << adjustable_highpass_cutoff << " Hz)" << std::endl;

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

    // Set up key callback for pause/resume and filter adjustment
    glfwSetKeyCallback(window, key_callback);

    const GLubyte* version = glGetString(GL_VERSION);
    if (!version) {
        std::cerr << "Failed to get OpenGL version - OpenGL context might not be properly initialized" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    std::cout << "OpenGL Version: " << version << std::endl;

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    if (!renderer || !vendor) {
        std::cerr << "Failed to get OpenGL renderer or vendor information" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    std::cout << "Renderer: " << renderer << std::endl;
    std::cout << "Vendor: " << vendor << std::endl;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    check_gl_error("glClearColor");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.5, 1.5, -1.0, 1.0);
    check_gl_error("glOrtho");
    glMatrixMode(GL_MODELVIEW);
    check_gl_error("glMatrixMode");

    std::cout << "OpenGL setup complete, entering main loop..." << std::endl;

    // Create filter instances
    Filter highPassFilter(FilterType::HighPass, SAMPLE_RATE, adjustable_highpass_cutoff);
    Filter bandPassFilter(FilterType::BandPass, SAMPLE_RATE, BANDPASS_LOW, BANDPASS_HIGH);
    Filter lowPassFilter(FilterType::LowPass, SAMPLE_RATE, LOWPASS_CUTOFF);

    float t = 0.0f;
    float last_highpass_cutoff = adjustable_highpass_cutoff; // Track the last cutoff to detect changes

    while (!glfwWindowShouldClose(window)) {
        auto start = std::chrono::high_resolution_clock::now();

        // Reinitialize high-pass filter if the cutoff has changed
        if (adjustable_highpass_cutoff != last_highpass_cutoff) {
            highPassFilter = Filter(FilterType::HighPass, SAMPLE_RATE, adjustable_highpass_cutoff);
            last_highpass_cutoff = adjustable_highpass_cutoff;
            std::cout << "High-pass filter reinitialized with cutoff: " << adjustable_highpass_cutoff << " Hz" << std::endl;
        }

        // Only update signals if not paused
        if (!is_paused) {
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
        }

        render_signals();
        glfwSwapBuffers(window);
        glfwPollEvents();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        if (buffer_index % 100 == 0) {
            std::cout << "Frame time: " << duration << " microseconds" << std::endl;
        }
    }

    std::cout << "Cleaning up..." << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Program exited successfully" << std::endl;
    return 0;
}