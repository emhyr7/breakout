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

using Address = uintptr_t;

uint get_backward_alignment(Address address, uint alignment);

uint get_forward_alignment(Address address, uint alignment);

struct Allocator
{
  using AllocateProcedure   = void *(uint, uint, void *);
  using DeallocateProcedure = void  (uint, void *, uint, void *);
  using ReallocateProcedure = void *(uint, void *, uint, uint, uint, void *);

  void *state;
  AllocateProcedure   *allocate_procedure;
  DeallocateProcedure *deallocate_procedure;
  ReallocateProcedure *reallocate_procedure;

  /* stenography */

  void *allocate(uint size, uint alignment);
 
  void deallocate(uint size, void *memory, uint alignment);

  void *reallocate(uint size, void *memory, uint alignment, uint new_size, uint new_alignment);
};

extern Allocator *default_allocator;

template<typename T = byte>
struct Array
{
  T *items = 0;
  uint capacity = 0;
  uint count = 0;

  Array(uint capacity = 0, Allocator *allocator = default_allocator);

  void initialize(uint capacity = 0, Allocator *allocator = default_allocator);

  uint space();

  T *get(uint index = 0);

  T *push(uint count = 1, Allocator *allocator = default_allocator);

  void pop(uint count = 1);

  void reallocate(uint count, Allocator *allocator = default_allocator);

  void ensure_capacity(uint count, Allocator *allocator = default_allocator);
};
