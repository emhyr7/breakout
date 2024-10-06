#include "breakout_allocators.h"

void *Allocator::allocate(uint size, uint alignment)
{
  return this->allocate_procedure(size, alignment, this->state);
}

void Allocator::deallocate(void *memory, uint size)
{
  return this->deallocate_procedure(memory, size, this->state);
}

void *Allocator::reallocate(void *memory, uint size, uint new_size, uint new_alignment)
{
  return this->reallocate_procedure(memory, size, size, new_alignment, this->state);
}

void *Allocator::push(uint size, uint alignment)
{
  return this->push_procedure(size, alignment, this->state);
}

void Allocator::derive(void *derivative)
{
  this->derive_procedure(derivative, this->state);
}

void Allocator::revert(void *derivative)
{
  this->revert_procedure(derivative, this->state);
}

template<typename T>
T *Linear_Allocator_Derivative::push(uint count, uint alignment)
{
  return this->allocator->push<T>(count, alignment);
}

void Linear_Allocator_Derivative::derive(Linear_Allocator_Derivative *derivative)
{
  this->allocator->derive(derivative);
}

void Linear_Allocator_Derivative::revert(Linear_Allocator_Derivative *derivative)
{
  this->allocator->revert(derivative);
}

void Linear_Allocator_Derivative::die(void)
{
  this->allocator->revert(this);
}

template<typename T>
T *Linear_Allocator::push(uint count, uint alignment)
{
  uint size = count * sizeof(T);
  uint forward_alignment = this->block ? get_forward_alignment((Address)this->block->memory + this->block->mass, alignment) : 0;
  if (!this->block || this->block->mass + forward_alignment + size > this->block->size)
  {
    this->minimum_block_size = get_maximum(this->default_minimum_block_size, this->minimum_block_size);
    uint new_block_size = get_maximum(size, this->minimum_block_size);
    Memory_Block *new_block = (Memory_Block *)context->allocator->allocate(sizeof(*new_block) + new_block_size, alignof(Memory_Block));
    new_block->size = new_block_size;
    new_block->mass = 0;
    new_block->memory = new_block->tailing_memory;
    new_block->prior = this->block;
    this->block = new_block;
  }
  void *memory = this->block->memory + this->block->mass + forward_alignment;
  if (this->enabled_nullify) fill_memory(0, memory, size);
  this->block->mass += forward_alignment + size;
  return (T *)memory;
}

void Linear_Allocator::derive(Linear_Allocator_Derivative *derivative)
{
  derivative->allocator = this;
  derivative->block = this->block;
  derivative->mass = this->block ? this->block->mass : 0;
}

void Linear_Allocator::revert(Linear_Allocator_Derivative *derivative)
{
  assert(derivative->allocator);
  for (;;)
  {
    if (this->block == derivative->block) break;
    Memory_Block *prior = this->block->prior;
    context->allocator->deallocate(this->block, sizeof(Memory_Block) + this->block->size);
    this->block = prior;
  }
  assert(block == derivative->block);
  if (block) this->block->mass = derivative->mass;
}
