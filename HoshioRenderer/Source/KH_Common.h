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
#include "ranges"


// ================= OpenGL / Window =================
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "imgui/ImGuizmo.h"

// ================= 数学库 =================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/component_wise.hpp>
#include "Eigen/StdList"
#include "Eigen/StdVector"
#include "Eigen/Sparse"

// ================= 辅助库 ==================
#include "magic_enum/magic_enum.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <tinyxml2/tinyxml2.h>


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


template <>
struct std::formatter<glm::ivec2> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const glm::ivec2& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", v.x, v.y);
    }
};

template <>
struct std::formatter<glm::vec2> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const glm::vec2& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f})", v.x, v.y);
    }
};

template <>
struct std::formatter<glm::vec3> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const glm::vec3& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z);
    }
};


