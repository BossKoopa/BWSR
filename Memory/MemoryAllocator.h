#ifndef __MEMORY_ALLOCATOR_H__
#define __MEMORY_ALLOCATOR_H__

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>

#include "utility/utility.h"
#include "utility/error.h"

// -----------------------------------------------------------------------------
//  STRUCTURES & DEFINITIONS
// -----------------------------------------------------------------------------

typedef struct memory_range_t {
    // Starting address of the memory range
    uintptr_t           Start;
    // Size of the memory range in bytes
    size_t              Size;
} memory_range_t;

typedef struct allocator_t {
    // Pointer to the allocated memory buffer
    uint8_t*            Buffer;
    // Current size of the allocated buffer in bytes
    uint32_t            Size;
    // Total capacity of the allocated buffer in bytes
    uint32_t            Capacity;
    // Page alignment requirement for allocations (typically 8 or 0)
    uint32_t            BuiltinAlignment;
} allocator_t;

typedef struct memory_allocator_t {
    // Pointer to all the allocators in use
    allocator_t*        Allocators;
    // Current amount of allocators
    size_t              AllocatorCount;
} memory_allocator_t;

// -----------------------------------------------------------------------------
//  EXPORTED FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * \brief Creates a memory block of a given size with `PROT_READ` and
 * `PROT_EXEC` permission.
 * \param[in,out]       MemoryRange         Block of allocated memory
 * \param[in,out]       Allocator           Allocator used to hold the memory
 * \param[in]           BufferSize          Size required of the memory block
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Allocator` or `MemoryRange` is `NULL`.
 * \retval `ERROR_INVALID_ARGUMENT_VALUE` if `BufferSize` is not greater than `0`.
 * \retval `ERROR_MEMORY_MAPPING` if `mmap` fails.
 * \retval `ERROR_MEMORY_PERMISSION` if the page permission could not be set
 * \retval `ERROR_MEM_ALLOC` on allocation failure
 * \retval `ERROR_MEMORY_OVERFLOW` if the allocator does not have enough memory available
 * \retval `ERROR_SUCCESS` if `MemoryRange` was allocated with the correct permissions
 */
BWSR_STATUS
    MemoryAllocator_AllocateExecutionBlock
    (
        IN  OUT     memory_range_t**        MemoryRange,
        IN  OUT     memory_allocator_t*     Allocator,
        IN          size_t                  BufferSize
    );

#endif // __MEMORY_ALLOCATOR_H__
