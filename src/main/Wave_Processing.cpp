#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

const int WIDTH = 800, HEIGHT = 600;
const int SAMPLES = 1000;
const float SAMPLE_RATE = 100000.0f; // 100 kHz
const float TIME_STEP = 1.0f / SAMPLE_RATE;