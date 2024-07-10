
#ifndef __MEMORY_TRACKER_H__
#define __MEMORY_TRACKER_H__

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "utility/utility.h"

// -----------------------------------------------------------------------------
//  DEFINITIONS
// -----------------------------------------------------------------------------

#if defined( DEBUG_MODE )

    // Memory tracked malloc
    #define BwsrMalloc( AllocationSize )                \
        MemoryTracker_Allocate( AllocationSize,         \
                                __FILE__,               \
                                __LINE__ )

    // Memory tracker realloc
    #define BwsrRealloc( Pointer, AllocationSize )      \
        MemoryTracker_Reallocate( Pointer,              \
                                  AllocationSize,       \
                                  __FILE__,             \
                                  __LINE__ )

    // Memory tracked calloc
    #define BwsrCalloc( AllocationCount, AllocationSize )   \
        MemoryTracker_Calloc( AllocationCount,              \
                              AllocationSize,               \
                              __FILE__,                     \
                              __LINE__ )

    // Memory tracked free
    #define BwsrFree( Pointer ) \
        MemoryTracker_Free( Pointer )

#else

    // Memory tracked malloc
    #define BwsrMalloc( AllocationSize ) \
        malloc( AllocationSize )

    // Memory tracker realloc
    #define BwsrRealloc( Pointer, AllocationSize ) \
        realloc( Pointer, AllocationSize )

    // Memory tracked calloc
    #define BwsrCalloc( AllocationCount, AllocationSize ) \
        calloc( AllocationCount, AllocationSize )

    // Memory tracked free
    #define BwsrFree( Pointer ) \
        if( NULL != Pointer )   \
        {                       \
            free( Pointer );    \
        }

#endif

// -----------------------------------------------------------------------------
//  EXPORTED FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * \brief Releases both the tracker and the allocation at a given address
 * \param[in]           Pointer             The pointer of the allocated memory
 * \return void
 */
void
    MemoryTracker_Free
    (
        IN          void*                   Pointer
    );

/**
 * \brief Reallocates memory at the given address and updates the tracker
 * \param[in,out]       Reference           The pointer to the allcoated memory
 * \param[in]           AllocationSize      The updated allocation size
 * \param[in]           FileName            The file name of the originator
 * \param[in]           LineNumber          The line number of the originator
 * \return void*
 * \retval NULL if any allocation fails.
 * \retval NULL if `Reference` or `FileName` is `NULL`.
 * \retval NULL if `AllocationSize` or `LineNumber` is not greater than `0`.
 * \retval void* a pointer to the new allocation
 */
void*
    MemoryTracker_Reallocate
    (
        IN  OUT     void*                   Reference,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    );

/**
 * \brief Allocates memory of the specified size and initializes a tracker
 * \param[in]           AllocationCount     The number of objects
 * \param[in]           AllocationSize      The size of each object
 * \param[in]           FileName            The file name of the originator
 * \param[in]           LineNumber          The line number of the originator
 * \return void*
 * \retval NULL if any allocation fails.
 * \retval NULL if `FileName` is `NULL`.
 * \retval NULL if `AllocationCount`, `AllocationSize` or `LineNumber` is not greater than `0`.
 * \retval void* a pointer to the new allocation
 */
void*
    MemoryTracker_Calloc
    (
        IN          const size_t            AllocationCount,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    );

/**
 * \brief Allocates memory of the specified size and initializes a tracker
 * \param[in]           AllocationSize      The size of the allcation
 * \param[in]           FileName            The file name of the originator
 * \param[in]           LineNumber          The line number of the originator
 * \return void*
 * \retval NULL if any allocation fails.
 * \retval NULL if `FileName` is `NULL`.
 * \retval NULL if `AllocationSize` or `LineNumber` is not greater than `0`.
 * \retval void* a pointer to the new allocation
 */
void*
    MemoryTracker_Allocate
    (
        IN          const uint64_t          AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    );

/**
 * \brief Iterates the memory tracker and counts the remaining allocations
 * \return size_t
 * \retval size_t A count of the remaining allocations in the memory tracker
 */
size_t
    MemoryTracker_CheckForMemoryLeaks
    (
        void
    );

#endif // __MEMORY_TRACKER_H__