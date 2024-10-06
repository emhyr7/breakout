#pragma once

#include "breakout.h"

struct Allocator
{
  using Allocate_Procedure   = void *(uint size, uint alignment, void *state);
  using Deallocate_Procedure = void  (void *memory, uint size, void *state);
  using Reallocate_Procedure = void *(void *memory, uint size, uint new_size, uint new_alignment, void *state);
  using Push_Procedure   = void *(uint size, uint alignment, void *state);
  using Derive_Procedure = void  (void *derivative, void *state);
  using Revert_Procedure = void  (void *derivative, void *state);

  void *state = 0;
  Allocate_Procedure   *allocate_procedure   = 0;
  Deallocate_Procedure *deallocate_procedure = 0;
  Reallocate_Procedure *reallocate_procedure = 0;
  Push_Procedure       *push_procedure       = 0;
  Derive_Procedure     *derive_procedure     = 0;
  Revert_Procedure     *revert_procedure     = 0;

  /* stenography */

  void *allocate(uint size, uint alignment);
 
  void deallocate(void *memory, uint size);

  void *reallocate(void *memory, uint size, uint new_size, uint new_alignment);

  void *push(uint size, uint alignment);

  void derive(void *derivative);

  void revert(void *derivative);
};

struct Memory_Block
{
  uint size;
  uint mass;
  byte *memory;
  Memory_Block *prior;
  alignas(universal_alignment) byte tailing_memory[];
};

struct Linear_Allocator;

/* ehh... kinda too wordy */
struct Linear_Allocator_Derivative
{
  Linear_Allocator *allocator;
  Memory_Block *block;
  uint mass;

  template<typename T = byte>
  T *push(uint count, uint alignment = alignof(T));

  void derive(Linear_Allocator_Derivative *derivative);

  void revert(Linear_Allocator_Derivative *derivative);

  void die();
};

struct Linear_Allocator
{
  static constexpr uint default_minimum_block_size = memory_page_size - sizeof(Memory_Block);
  
  Memory_Block *block = 0;

  /* options */
  uint minimum_block_size = this->default_minimum_block_size;
  bool enabled_nullify : 1 = true;

  template<typename T = byte>
  T *push(uint count, uint alignment = alignof(T));

  void derive(Linear_Allocator_Derivative *derivative);

  void revert(Linear_Allocator_Derivative *derivative);
};
