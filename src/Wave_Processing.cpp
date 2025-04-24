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
const int WINDOW_HEIGHT = 600;
const int BUFFER_SIZE = 1000; // Number of samples to display
const float SAMPLE_RATE = 2000.0f; // Hz
const float TIME_STEP = 1.0f / SAMPLE_RATE;

// Synthetic EMG signal parameters
const float EMG_FREQ = 20.0f; // Base EMG frequency (Hz)
const float NOISE_AMPLITUDE = 0.2f;
const float POWER_LINE_FREQ = 10.0f; // Power line interference frequency (Hz)
const float POWER_LINE_AMPLITUDE = 0.3f;

// Filter parameters
const float BANDPASS_LOW = 5.0f; // Hz
const float BANDPASS_HIGH = 50.0f; // Hz
const float NOTCH_FREQ = 10.0f; // Hz
const float NOTCH_Q = 30.0f; // Quality factor for notch filter (higher Q = narrower notch)

// Circular buffer for raw and filtered signals
std::vector<float> raw_signal(BUFFER_SIZE, 0.0f);
std::vector<float> filtered_signal(BUFFER_SIZE, 0.0f);
int buffer_index = 0;

// Random number generator for noise and EMG variation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(-NOISE_AMPLITUDE, NOISE_AMPLITUDE); // For background noise
std::uniform_real_distribution<float> amp_dist(0.8f, 1.2f); // For amplitude variation (0.8 to 1.2)
std::uniform_real_distribution<float> freq_dist(0.9f, 1.1f); // For frequency variation (0.9 to 1.1)
std::uniform_real_distribution<float> burst_dist(0.0f, 1.0f); // For random burst probability
std::uniform_real_distribution<float> burst_scale(1.0f, 3.0f); // For burst amplitude scaling

// Function to check OpenGL errors
void check_gl_error() {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error: " << error << std::endl;
    }
}

// Generate synthetic EMG signal with randomness
float generate_emg_signal(float t) {
    static float phase1 = 0.0f; // Phase for first EMG component
    static float phase2 = 0.0f; // Phase for second EMG component
    static float burst_factor = 1.0f; // Burst scaling factor
    static int burst_duration = 0; // Counter for burst duration

    // Update burst behavior every 50 samples (approximately every 25ms at 2000 Hz)
    if (buffer_index % 50 == 0) {
        // Randomly decide if a burst occurs (20% chance)
        if (burst_dist(gen) < 0.2f) {
            burst_factor = burst_scale(gen); // Scale amplitude randomly between 1x and 3x
            burst_duration = 100; // Burst lasts for 100 samples (50ms)
        } else if (burst_duration <= 0) {
            burst_factor = 1.0f; // Reset to normal amplitude
        }
    }
    if (burst_duration > 0) {
        burst_duration--;
    }

    // Randomly vary amplitude and frequency
    float amp1 = 0.5f * amp_dist(gen); // Random amplitude for first component
    float amp2 = 0.3f * amp_dist(gen); // Random amplitude for second component
    float freq1 = EMG_FREQ * freq_dist(gen); // Random frequency variation for first component
    float freq2 = (EMG_FREQ * 1.5f) * freq_dist(gen); // Random frequency variation for second component

    // Update phases
    phase1 += 2.0f * M_PI * freq1 * TIME_STEP;
    phase2 += 2.0f * M_PI * freq2 * TIME_STEP;

    // Generate EMG signal with random variations and bursts
    float emg = burst_factor * (amp1 * sin(phase1) + amp2 * sin(phase2));

    // Add power line interference
    emg += POWER_LINE_AMPLITUDE * sin(2.0f * M_PI * POWER_LINE_FREQ * t);

    // Add random noise
    emg += dist(gen);

    return emg;
}

// Second-order Butterworth band-pass filter
float bandpass_filter(float input, float& x1, float& x2, float& y1, float& y2) {
    float wc = 2.0f * M_PI * sqrt(BANDPASS_LOW * BANDPASS_HIGH) / SAMPLE_RATE;
    float bw = 2.0f * M_PI * (BANDPASS_HIGH - BANDPASS_LOW) / SAMPLE_RATE;

    float alpha = sin(bw) * sinh(log(2.0f) / 2.0f * 1.0f * M_PI / 2.0f);
    float cosw = cos(wc);

    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw;
    float a2 = 1.0f - alpha;
    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    a0 = 1.0f;

    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

// Notch filter to remove power line noise
float notch_filter(float input, float& x1, float& x2, float& y1, float& y2) {
    float omega = 2.0f * M_PI * NOTCH_FREQ / SAMPLE_RATE;
    float alpha = sin(omega) / (2.0f * NOTCH_Q);

    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cos(omega);
    float a2 = 1.0f - alpha;
    float b0 = 1.0f;
    float b1 = -2.0f * cos(omega);
    float b2 = 1.0f;

    float output = (b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

// Normalize signal for visualization
float normalize_for_display(float value, float max_amplitude) {
    const float display_range = 0.8f;
    if (max_amplitude == 0.0f) return value;
    return (value / max_amplitude) * display_range;
}

// Render signals using OpenGL
void render_signals() {
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error();

    float max_raw = 0.0f;
    float max_filtered = 0.0f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        max_raw = std::max(max_raw, std::abs(raw_signal[i]));
        max_filtered = std::max(max_filtered, std::abs(filtered_signal[i]));
    }

    glColor3f(1.0f, 0.0f, 0.0f);
    check_gl_error();
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = normalize_for_display(raw_signal[(buffer_index + i) % BUFFER_SIZE], max_raw) + 0.5f;
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = normalize_for_display(filtered_signal[(buffer_index + i) % BUFFER_SIZE], max_filtered) - 0.5f;
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glEnd();
    check_gl_error();
}

int main() {
    std::cout << "Starting program..." << std::endl;
    std::cout << "Red (Top): Initial Signal (Raw EMG)" << std::endl;
    std::cout << "Green (Bottom): Output Signal (Filtered EMG)" << std::endl;

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    std::cout << "GLFW initialized successfully" << std::endl;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "EMG Signal Filtering (Red: Raw, Green: Filtered)", nullptr, nullptr);
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
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    check_gl_error();

    std::cout << "OpenGL setup complete, entering main loop..." << std::endl;

    float x1_bp = 0.0f, x2_bp = 0.0f, y1_bp = 0.0f, y2_bp = 0.0f;
    float x1_n = 0.0f, x2_n = 0.0f, y1_n = 0.0f, y2_n = 0.0f;
    float t = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float raw = generate_emg_signal(t);
        float filtered = bandpass_filter(raw, x1_bp, x2_bp, y1_bp, y2_bp);
        filtered = notch_filter(filtered, x1_n, x2_n, y1_n, y2_n);

        raw_signal[buffer_index] = raw;
        filtered_signal[buffer_index] = filtered;
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;

        t += TIME_STEP;

        if (buffer_index % 100 == 0) {
            float max_raw = *std::max_element(raw_signal.begin(), raw_signal.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
            float max_filtered = *std::max_element(filtered_signal.begin(), filtered_signal.end(), [](float a, float b) { return std::abs(a) < std::abs(b); });
            std::cout << "Max Raw Amplitude: " << max_raw << ", Max Filtered Amplitude: " << max_filtered << std::endl;
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