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

Allocator *default_allocator;

void *Allocator::allocate(uint size, uint alignment)
{
  return this->allocate_procedure(size, alignment, this->state);
}

void Allocator::deallocate(uint size, void *memory, uint alignment)
{
  return this->deallocate_procedure(size, memory, alignment, this->state);
}

void *Allocator::reallocate(uint size, void *memory, uint alignment, uint new_size, uint new_alignment)
{
  return this->reallocate_procedure(size, memory, alignment, size, new_alignment, this->state);
}

template<typename T>
Array<T>::Array(uint capacity, Allocator *allocator)
{
  this->initialize(capacity, allocator);
}

template<typename T>
void Array<T>::initialize(uint capacity, Allocator *allocator)
{
  this->items    = 0;
  this->capacity = 0;
  this->count    = 0;
  this->ensure_capacity(capacity, allocator);
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
T *Array<T>::push(uint count, Allocator *allocator)
{
  this->ensure_capacity(count, allocator);
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
void Array<T>::reallocate(uint count, Allocator *allocator)
{
  this->items = (T *)allocator->reallocate(this->count * sizeof(T), this->items, alignof(T), count * sizeof(T), alignof(T));
}

template<typename T>
void Array<T>::ensure_capacity(uint count, Allocator *allocator)
{
  if (count > this->space())
  {
    this->capacity += count + this->capacity / 2;
    this->reallocate(this->capacity, allocator);
  }
}
