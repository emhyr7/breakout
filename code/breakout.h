#pragma once

#if defined(_DEBUG) || !defined(NDEBUG)
  #define DEBUGGING
#endif

#define UNICODE
#if defined(UNICODE)
  #define _UNICODE
#endif

#if defined(UNICODE)
  #define USING_UNICODE
#endif

#include <Windows.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#define countof(x) (sizeof(x) / sizeof(x[0]))
