#pragma once

// ================= 标准库 =================
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <functional>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <omp.h>
#include <random>
#include <chrono>
#include <format>
#include <mutex>

#include <sstream>
#include <filesystem>
#include <iomanip>

// ================= OpenGL / Window =================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

// ================= 数学库 =================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Eigen/StdList"
#include "Eigen/StdVector"
#include "Eigen/Sparse"

// ================= 辅助库 ==================
#include "tiny_obj_loader/tiny_obj_loader.h"

#include "magic_enum/magic_enum.hpp"

// ================= Macro ==================
#define EPS 1e-6
#define PI 3.1415926

// ================ Template ================
template <typename T>
class KH_Singleton {
public:
    static T& Instance() {
        static T instance;
        return instance;
    }

    KH_Singleton(const KH_Singleton&) = delete;
    KH_Singleton& operator=(const KH_Singleton&) = delete;

protected:
    KH_Singleton() = default;
    virtual ~KH_Singleton() = default;
};


