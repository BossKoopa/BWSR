
// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <limits.h>

#ifdef BWSR_SECURELY_ZERO_MEMORY

    #include <string.h>

#endif // BWSR_SECURELY_ZERO_MEMORY

#include "Memory/MemoryTracker.h"
#include "utility/debug.h"
#include "utility/error.h"

// -----------------------------------------------------------------------------
//  STRUCTURES & DEFINITIONS
// -----------------------------------------------------------------------------

#define FREE_ALLOCATION_POINTER 1
#define KEEP_ALLOCATION_POINTER 0

/**
 * \brief Tracks file name and line number of allocations
 */
typedef struct memory_tracker_t {
    // Address of the allocated memory
    void*                       Address;
    // The size, in bytes, of the allocated memory
    size_t                      AllocationSize;
    // The location of the next memory tracker instance
    struct memory_tracker_t*    Next;
    // The location of the previous memory tracker instance
    struct memory_tracker_t*    Previous;
    // The line number of the originator requesting allocation
    size_t                      LineNumber;
    // The file name of the originator requesting allocation
    char                        FileName[ 1 ];
} memory_tracker_t;

// -----------------------------------------------------------------------------
//  GLOBALS
// -----------------------------------------------------------------------------

static memory_tracker_t gMemoryTracker =
{
    .Address            = 0,
    .AllocationSize     = 0,
    .Next               = &gMemoryTracker,
    .Previous           = &gMemoryTracker
};

// -----------------------------------------------------------------------------
//  PROTOTYPES
// -----------------------------------------------------------------------------

/**
 * \brief Releases a memory tracker and removes it from the global link
 * \param[in,out]       Tracker             The Tracker of a memory allocation
 * \param[in]           ReleaseAddress      Wether not to release the allocation
 * \return void
 */
static
void
    INTERNAL_MemoryTracker_Release
    (
        IN  OUT     memory_tracker_t*       Tracker,
        IN          const uint8_t           ReleaseAddress
    );

/**
 * \brief Initializes a memory tracker and adds it to the global link
 * \param[in,out]       Tracker             Pointer to track memory allocation
 * \param[in]           AllocationSize      The requested allocation size
 * \param[in]           FileName            The file name of the originator
 * \param[in]           LineNumber          The line number of the originator
 * \return BWSR_STATUS
 * \retval ERROR_ARGUMENT_IS_NULL if `Tracker` or `FileName` is `NULL`.
 * \retval ERROR_INVALID_ARGUMENT_VALUE if `AllocationSize` or `LineNumber` is not greater than `0`.
 * \retval ERROR_MEM_ALLOC if `Tracker` could not be allocated
 * \retval ERROR_SUCCESS if `Tracker` was allocated and linked in `gMemoryTracker`.
 */
static
BWSR_STATUS
    INTERNAL_MemoryTracker_Initialize
    (
        OUT         memory_tracker_t**      Tracker,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    );

// -----------------------------------------------------------------------------
//  IMPLEMENTATION
// -----------------------------------------------------------------------------

static
void
    INTERNAL_MemoryTracker_Release
    (
        IN  OUT     memory_tracker_t*       Tracker,
        IN          const uint8_t           ReleaseAddress
    )
{
    __NOT_NULL_RETURN_VOID( Tracker );

    if( ( FREE_ALLOCATION_POINTER == ReleaseAddress   ) &&
        ( NULL                    != Tracker->Address ) )
    {

#ifdef BWSR_SECURELY_ZERO_MEMORY

        memset_s( Tracker->Address,
                  Tracker->AllocationSize,
                  0,
                  Tracker->AllocationSize );

#endif // BWSR_SECURELY_ZERO_MEMORY

        free( Tracker->Address );
        Tracker->Address = NULL;
    } // Tracking check

    Tracker->Previous->Next     = Tracker->Next;
    Tracker->Next->Previous     = Tracker->Previous;
    Tracker->Next               = NULL;
    Tracker->Previous           = NULL;
    Tracker->AllocationSize     = 0;

    free( Tracker );
}

static
BWSR_STATUS
    INTERNAL_MemoryTracker_Initialize
    (
        OUT         memory_tracker_t**      Tracker,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    size_t          lineLength      = 0;

    __NOT_NULL( Tracker, FileName );
    __GREATER_THAN_0( AllocationSize, LineNumber );

    lineLength = strnlen( FileName, PATH_MAX );

    if( NULL == ( *Tracker = malloc( sizeof( memory_tracker_t ) + lineLength + sizeof( char ) ) ) )
    {
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        memcpy( ( *Tracker )->FileName,
                FileName,
                lineLength );

        *( ( *Tracker )->FileName + lineLength )    = 0x00;
        ( *Tracker )->AllocationSize                = AllocationSize;
        ( *Tracker )->LineNumber                    = LineNumber;
        ( *Tracker )->Next                          = &gMemoryTracker;
        ( *Tracker )->Previous                      = gMemoryTracker.Previous;
        gMemoryTracker.Previous->Next               = *Tracker;
        gMemoryTracker.Previous                     = *Tracker;

        retVal = ERROR_SUCCESS;
    } // malloc()

    return retVal;
}

void
    MemoryTracker_Free
    (
        IN          void*                   Pointer
    )
{
    memory_tracker_t* tracker = gMemoryTracker.Next;

    __NOT_NULL_RETURN_VOID( Pointer );

    while( ( tracker          != &gMemoryTracker ) &&
           ( tracker->Address != Pointer         ) )
    {
        tracker = tracker->Next;
    } // while()

    if( tracker == &gMemoryTracker )
    {
        BWSR_DEBUG( LOG_INFO,
                    "Not tracking address: %p. Not attempting release.\n",
                    Pointer );
    }
    else {
        INTERNAL_MemoryTracker_Release( tracker, FREE_ALLOCATION_POINTER );
    } // dummy check

    return;
}

void*
    MemoryTracker_Reallocate
    (
        IN  OUT     void*                   Reference,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    )
{
    memory_tracker_t*   tracker         = gMemoryTracker.Next;
    void*               allocation      = NULL;

    __NOT_NULL_RETURN_NULL( Reference, FileName );
    __GREATER_THAN_0_RETURN_NULL( AllocationSize, LineNumber );

    while( ( tracker          != &gMemoryTracker ) &&
           ( tracker->Address != Reference       ) )
    {
        tracker = tracker->Next;
    } // while()

    if( tracker == &gMemoryTracker )
    {
        BWSR_DEBUG( LOG_CRITICAL, "You don't know what you're doing.\n" );
    }
    else {
        INTERNAL_MemoryTracker_Release( tracker, KEEP_ALLOCATION_POINTER );

        if( ERROR_SUCCESS != INTERNAL_MemoryTracker_Initialize( &tracker,
                                                                AllocationSize,
                                                                FileName,
                                                                LineNumber ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_MemoryTracker_Initialize() Failed\n" );
        }
        else {
            if( NULL == ( allocation = realloc( Reference, AllocationSize ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "realloc() Failed\n" );
                INTERNAL_MemoryTracker_Release( tracker, KEEP_ALLOCATION_POINTER );
            }
            else {
                tracker->Address = allocation;
            } // realloc()
        } // INTERNAL_MemoryTracker_Initialize()
    } // dummy check

    return allocation;
}

void*
    MemoryTracker_Calloc
    (
        IN          const size_t            AllocationCount,
        IN          const size_t            AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    )
{
    memory_tracker_t*   tracker         = NULL;
    void*               allocation      = NULL;

    __NOT_NULL_RETURN_NULL( FileName );
    __GREATER_THAN_0_RETURN_NULL( AllocationCount,
                                  AllocationSize,
                                  LineNumber );

    if( ERROR_SUCCESS != INTERNAL_MemoryTracker_Initialize( &tracker,
                                                            ( AllocationSize * AllocationCount ),
                                                            FileName,
                                                            LineNumber ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_MemoryTracker_Initialize() Failed\n" );
    }
    else {
        if( NULL == ( allocation = calloc( AllocationCount, AllocationSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "malloc() Failed\n" );
            INTERNAL_MemoryTracker_Release( tracker, KEEP_ALLOCATION_POINTER );
        }
        else {
            tracker->Address = allocation;
        } // malloc()
    } // INTERNAL_MemoryTracker_Initialize()

    return allocation;
}

void*
    MemoryTracker_Allocate
    (
        IN          const uint64_t          AllocationSize,
        IN          const char*             FileName,
        IN          const size_t            LineNumber
    )
{
    memory_tracker_t*   tracker         = NULL;
    void*               allocation      = NULL;

    __NOT_NULL_RETURN_NULL( FileName );
    __GREATER_THAN_0_RETURN_NULL( AllocationSize, LineNumber );

    if( ERROR_SUCCESS != INTERNAL_MemoryTracker_Initialize( &tracker,
                                                            AllocationSize,
                                                            FileName,
                                                            LineNumber ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_MemoryTracker_Initialize() Failed\n" );
    }
    else {
        if( NULL == ( allocation = malloc( AllocationSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "malloc() Failed\n" );
            INTERNAL_MemoryTracker_Release( tracker, KEEP_ALLOCATION_POINTER );
        }
        else {
            tracker->Address = allocation;
        } // malloc()
    } // INTERNAL_MemoryTracker_Initialize()

    return allocation;
}

size_t
    MemoryTracker_CheckForMemoryLeaks
    (
        void
    )
{
    memory_tracker_t*   tracker         = gMemoryTracker.Next;
    size_t              leakCount       = 0;

#ifdef DEBUG_MODE
    size_t              leakAmount      = 0;
#endif

    while( tracker != &gMemoryTracker )
    {
        leakCount++;

        BWSR_DEBUG( LOG_WARNING,
                    "%s[%zu]: Leaked %zu Bytes at Address: %p!",
                    tracker->FileName,
                    tracker->LineNumber,
                    tracker->AllocationSize,
                    tracker->Address );
#ifdef DEBUG_MODE
        leakAmount += tracker->AllocationSize;
#endif
        tracker = tracker->Next;
    } // while()

#ifdef DEBUG_MODE
    if( leakCount )
    {
        BWSR_DEBUG( LOG_WARNING,
                    "Totals: %zu leaks totalling %zu Bytes!",
                    leakCount,
                    leakAmount );
    }
#endif

    return leakCount;
}
