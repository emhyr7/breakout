#include "breakout.h"

inline uint get_backward_alignment(Address address, uint alignment)
{
  return alignment ? address & (alignment - 1) : 0;
}

inline uint get_forward_alignment(Address address, uint alignment)
{
  uint modulus = get_backward_alignment(address, alignment);
  return modulus ? alignment - modulus : 0;
}

inline uint get_maximum(uint left, uint right)
{
  return left >= right ? left : right;
}

inline uint get_minimum(uint left, uint right)
{
  return left <= right ? left : right;
}

inline void fill_memory(byte value, void *memory, uint size)
{
  memset(memory, value, size);
}

thread_local Context *context;

#if 0 /* */

template<typename T>
void Array<T>::initialize(uint capacity)
{
  this->items    = 0;
  this->capacity = 0;
  this->count    = 0;
  this->ensure_capacity(capacity);
}

template<typename T>
uint Array<T>::space()
{
  return this->capacity - this->count;
}

template<typename T>
T *Array<T>::get(uint index)
{
  return this->items + index;
}

template<typename T>
T *Array<T>::push(uint count)
{
  this->ensure_capacity(count);
  T *result = this->items + this->count;
  this->count += count;
  return result;
}

template<typename T>
void Array<T>::pop(uint count)
{
  if (count > this->count) count = this->count;
  this->count -= count;
}

template<typename T>
void Array<T>::reallocate(uint count)
{
  this->items = (T *)context->allocator->reallocate(this->items, this->count * sizeof(T), count * sizeof(T), alignof(T));
}

template<typename T>
void Array<T>::ensure_capacity(uint count)
{
  if (count > this->space())
  {
    this->capacity += count + this->capacity / 2;
    this->reallocate(this->capacity);
  }
}

#endif
