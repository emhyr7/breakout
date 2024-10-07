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
#include <tchar.h>
#include <strsafe.h>

#define VK_USE_PLATFORM_WIN32_KHR 1
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

using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using sint8  = int8_t;
using sint16 = int16_t;
using sint32 = int32_t;
using sint64 = int64_t;

using uintb = uint8;
using uints = uint16;
using uint  = uint32;
using uintl = uint64;

constexpr uint uint32_maximum = ~(uint32)0;

using sintb = sint8;
using sints = sint16;
using sint  = sint32;
using sintl = sint64;

using utf8  = char;
using utf16 = wchar_t;
using utf32 = uint32;

constexpr uint universal_alignment = alignof(max_align_t);

constexpr uint memory_page_size = 4096;

using Address = uintptr_t;

template<typename T>
struct Span { T *items; uint count; };

uint get_backward_alignment(Address address, uint alignment);

uint get_forward_alignment(Address address, uint alignment);

uint get_maximum(uint left, uint right);

uint get_minimum(uint left, uint right);

uint clamp(uint value, uint minimum, uint maximum);

void fill_memory(byte value, void *memory, uint size);

uint get_string_length(const utf8 *string);

sint compare_string(const utf8 *left, const utf8 *right);

utf16 *make_terminated_utf16_string_from_utf8(uint *utf16_string_length, const utf8 *utf8_string);

using Handle = HANDLE;

Handle open_file(const char *path);

uintl get_size_of_file(Handle handle);

uint read_from_file(Handle handle);

Span<uintb> read_from_file_quickly(const char *path);

void close_file(Handle handle);

#include "breakout_allocators.h"

struct Context
{
  Linear_Allocator default_linear_allocator;
  Linear_Allocator *linear_allocator = &default_linear_allocator;
  Allocator default_allocator =
  {
    .state = this,
    .allocate_procedure   = [](uint size, uint alignment, void *state) -> void *{ return malloc(size); },
    .deallocate_procedure = [](void *memory, uint size, void *state) -> void { free(memory); },
    .reallocate_procedure = [](void *memory, uint size, uint new_size, uint new_alignment, void *state) -> void * { return realloc(memory, new_size); },
    .push_procedure       = [](uint size, uint alignment, void *state) -> void *{ return ((Context *)state)->linear_allocator->push(size, alignment); },
    .derive_procedure     = [](void *derivative, void *state) { ((Context *)state)->linear_allocator->derive((Linear_Allocator_Derivative *)derivative); },
    .revert_procedure     = [](void *derivative, void *state) { ((Context *)state)->linear_allocator->revert((Linear_Allocator_Derivative *)derivative); },
  };
  Allocator *allocator = &this->default_allocator;
};

extern thread_local Context context;

using Scratch = Linear_Allocator_Derivative;

#if 0 /* excluded since it's useless ATM */

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
