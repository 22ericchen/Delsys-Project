#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <random>
#include <iostream>

// Define M_PI for platforms where it's not defined
#define M_PI 3.14159265358979323846

// Constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int BUFFER_SIZE = 1000; // Number of samples to display
const float SAMPLE_RATE = 2000.0f; // Hz
const float TIME_STEP = 1.0f / SAMPLE_RATE;

// Synthetic EMG signal parameters
const float EMG_FREQ = 100.0f; // Dominant EMG frequency (Hz)
const float NOISE_AMPLITUDE = 0.2f;
const float POWER_LINE_FREQ = 60.0f;
const float POWER_LINE_AMPLITUDE = 0.3f;

// Filter coefficients (simplified IIR band-pass and notch)
const float BANDPASS_LOW = 20.0f; // Hz
const float BANDPASS_HIGH = 400.0f; // Hz
const float NOTCH_FREQ = 60.0f; // Hz
const float Q = 10.0f; // Quality factor for notch filter

// Circular buffer for raw and filtered signals
std::vector<float> raw_signal(BUFFER_SIZE, 0.0f);
std::vector<float> filtered_signal(BUFFER_SIZE, 0.0f);
int buffer_index = 0;

// Random number generator for noise
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(-NOISE_AMPLITUDE, NOISE_AMPLITUDE);

// Function to check OpenGL errors
void check_gl_error() {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error: " << error << std::endl;
    }
}

// Generate synthetic EMG signal
float generate_emg_signal(float t) {
    float emg = 0.5f * sin(2.0f * M_PI * EMG_FREQ * t) +
                0.3f * sin(2.0f * M_PI * (EMG_FREQ * 1.5f) * t);
    emg += POWER_LINE_AMPLITUDE * sin(2.0f * M_PI * POWER_LINE_FREQ * t);
    emg += dist(gen);
    return emg;
}

// Simple IIR band-pass filter (Butterworth approximation)
float bandpass_filter(float input, float& x1, float& x2, float& y1, float& y2) {
    float omega_low = 2.0f * M_PI * BANDPASS_LOW / SAMPLE_RATE;
    float omega_high = 2.0f * M_PI * BANDPASS_HIGH / SAMPLE_RATE;
    
    float a0 = 1.0f;
    float a1 = -2.0f * cos(omega_high);
    float a2 = 1.0f;
    float b0 = sin(omega_high - omega_low) / 2.0f;
    float b1 = 0.0f;
    float b2 = -b0;

    float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

// Notch filter to remove 60 Hz noise
float notch_filter(float input, float& x1, float& x2, float& y1, float& y2) {
    float omega = 2.0f * M_PI * NOTCH_FREQ / SAMPLE_RATE;
    float alpha = sin(omega) / (2.0f * Q);

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

// Render signals using OpenGL
void render_signals() {
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error();

    // Draw raw signal (top half)
    glColor3f(1.0f, 0.0f, 0.0f); // Red
    check_gl_error();
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = raw_signal[(buffer_index + i) % BUFFER_SIZE] * 0.2f + 0.5f;
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    // Draw filtered signal (bottom half)
    glColor3f(0.0f, 1.0f, 0.0f); // Green
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float x = -1.0f + 2.0f * i / (float)(BUFFER_SIZE - 1);
        float y = filtered_signal[(buffer_index + i) % BUFFER_SIZE] * 0.2f - 0.5f;
        glVertex2f(x, y);
    }
    glEnd();
    check_gl_error();

    // Draw separator line
    glColor3f(1.0f, 1.0f, 1.0f); // White
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glEnd();
    check_gl_error();
}

int main() {
    std::cout << "Starting program..." << std::endl;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    std::cout << "GLFW initialized successfully" << std::endl;

    // Create window with OpenGL 2.1 context
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

    // Check OpenGL version
    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        std::cout << "OpenGL Version: " << version << std::endl;
    } else {
        std::cerr << "Failed to get OpenGL version" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Check OpenGL renderer and vendor
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    std::cout << "Renderer: " << (renderer ? reinterpret_cast<const char*>(renderer) : "Unknown") << std::endl;
    std::cout << "Vendor: " << (vendor ? reinterpret_cast<const char*>(vendor) : "Unknown") << std::endl;

    // Set up OpenGL
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    check_gl_error();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    check_gl_error();

    std::cout << "OpenGL setup complete, entering main loop..." << std::endl;

    // Filter state variables
    float x1_bp = 0.0f, x2_bp = 0.0f, y1_bp = 0.0f, y2_bp = 0.0f; // Band-pass
    float x1_n = 0.0f, x2_n = 0.0f, y1_n = 0.0f, y2_n = 0.0f; // Notch

    // Time variable
    float t = 0.0f;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Generate new EMG sample
        float raw = generate_emg_signal(t);
        float filtered = bandpass_filter(raw, x1_bp, x2_bp, y1_bp, y2_bp);
        filtered = notch_filter(filtered, x1_n, x2_n, y1_n, y2_n);

        // Update buffers
        raw_signal[buffer_index] = raw;
        filtered_signal[buffer_index] = filtered;
        buffer_index = (buffer_index + 1) % BUFFER_SIZE;

        // Update time
        t += TIME_STEP;

        // Render
        render_signals();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    std::cout << "Cleaning up..." << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Program exited successfully" << std::endl;
    return 0;
}