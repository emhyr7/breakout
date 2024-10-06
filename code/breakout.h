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
#include <cstdio>
#include <stdexcept>

#define countof(x) (sizeof(x) / sizeof(x[0]))

using namespace glm;


using byte = uint8_t;

using uintb = uint8_t;
using uints = uint16_t;
using uint  = uint32_t;
using uintl = uint64_t;

constexpr uint universal_alignment = alignof(max_align_t);

constexpr uint memory_page_size = 4096;

using Address = uintptr_t;

uint get_backward_alignment(Address address, uint alignment);

uint get_forward_alignment(Address address, uint alignment);

uint get_maximum(uint left, uint right);

uint get_minimum(uint left, uint right);

void fill_memory(byte value, void *memory, uint size);

#include "breakout_allocators.h"

struct Context
{
  Linear_Allocator linear_allocator;
  Allocator        default_allocator;
  Allocator       *allocator;
};

extern thread_local Context *context;

using Scratch = Linear_Allocator_Derivative;

#if 0

template<typename T>
struct Array
{
  T *items;
  uint count;
  uint capacity;

  void initialize(uint capacity);

  uint space();

  T *get(uint index = 0);

  T *push(uint count = 1);

  void pop(uint count = 1);

  void reallocate(uint count);

  void ensure_capacity(uint count);
};

#endif
