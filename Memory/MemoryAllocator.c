
// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <unistd.h>
#include <sys/mman.h>

#if defined( __APPLE__ )
    #include <mach/mach.h>
#endif

#include "Memory/Memory.h"

// -----------------------------------------------------------------------------
//  STRUCTURES & DEFINITIONS
// -----------------------------------------------------------------------------

#define ALIGN_CEIL( ADDRESS, RANGE ) \
    ( ( (uintptr_t) ADDRESS + (uintptr_t) RANGE - 1 ) & ~( (uintptr_t)RANGE - 1 ) )

/**
 * \brief Enforces adherence to memory protection range.
 */
typedef enum MemoryPermission {
    kNoAccess,

    kRead               = 1,
    kWrite              = 2,
    kExecute            = 4,

    kReadWrite          = kRead | kWrite,
    kReadExecute        = kRead | kExecute,
    kReadWriteExecute   = kRead | kWrite | kExecute,
} MemoryPermission;

// -----------------------------------------------------------------------------
//  GLOBALS
// -----------------------------------------------------------------------------

#if defined(__APPLE__)
    const int kMmapFd = VM_MAKE_TAG(255);
#else
    const int kMmapFd = -1;
#endif

const int kMmapFdOffset = 0;

// -----------------------------------------------------------------------------
//  PROTOTYPES
// -----------------------------------------------------------------------------

/**
 * \brief Converts MemoryPermission to suitable access permission
 * \param[in]           Access              Access permssion to convert
 * \return int
 * \retval int of memory protections matching the ones definied by `Access`.
 */
static
int
    INTERNAL_GetPageProtection
    (
        IN          const MemoryPermission      Access
    );

/**
 * \brief Initializes an Allocator.
 * \param[in,out]       Allocator           Allocator instance to initialize.
 * \param[in]           VMPage              The Allocated virtual page
 * \param[in]           Capacity            The size of the virtual page
 * \param[in]           Alignment           The byte alignment of the page
 * \return void
 */
static
void
    INTERNAL_AllocatorInitialize
    (
        OUT         allocator_t*                Allocator,
        IN          uint8_t*                    VMPage,
        IN          uint32_t                    Capacity,
        IN          uint32_t                    Alignment
    );

/**
 * \brief Gets the next page offset in the buffer of the allocator.
 * \param[in,out]       Data                Pointer to update with the page offset
 * \param[in]           Allocator           The allocator holding page buffers
 * \param[in]           BufferSize          The size needed by `Data`
 * \param[in]           Alignment           The byte alignment of the virtual page.
 * \return BWSR_STATUS
 * \retval ERROR_MEMORY_OVERFLOW if the allocator does not have enough memory available
 * \retval ERROR_ARGUMENT_IS_NULL If `Data` or `Allocator` is `NULL`.
 * \retval ERROR_INVALID_ARGUMENT_VALUE If `BufferSize` is not greater than `0`.
 * \retval ERROR_SUCCESS if `Data` now points to a region of `Alloctor`'s buffer
 */
static
BWSR_STATUS
    INTERNAL_Allocator_GetNextOffsetInBuffer
    (
        OUT         uint8_t**                   Data,
        IN  OUT     allocator_t*                Allocator,
        IN          const uint32_t              BufferSize,
        IN          const uint32_t              Alignment
    );

/**
 * \brief Sets the protection permission on specific page buffer
 * \param[in]           Address             The address of the page buffer
 * \param[in]           PageSize            The size of the page buffer
 * \param[in]           Access              Permission to set the page buffer
 * \return BWSR_STATUS
 * \retval ERROR_ARGUMENT_IS_NULL if `Address` is `NULL`.
 * \retval ERROR_MEMORY_PERMISSION if the page permission could not be set
 * \retval ERROR_SUCCESS if the page permissions were changed
 */
static
BWSR_STATUS
    INTERNAL_SetPagePermission
    (
        IN  OUT     void*                       Address,
        IN          const size_t                PageSize,
        IN          const MemoryPermission      Access
    );

/**
 * \brief Allocates a virtual page with the given protection permissions
 * \param[in,out]       MemoryRegion        Address of mapped memory region
 * \param[in]           MappingLength       Length of the mapping
 * \param[in]           Access              Mapping options
 * \param[in]           FixedAddress        Start address of the mapping
 * \return BWSR_STATUS
 * \retval ERROR_ARGUMENT_IS_NULL if `MemoryRegion` is `NULL`.
 * \retval ERROR_INVALID_ARGUMENT_VALUE if `MappingLength` is not greater than `0`.
 * \retval ERROR_MEMORY_MAPPING if mmap fails.
 * \retval ERROR_SUCCESS if the page was allocated with the specified permissions
 */
static
BWSR_STATUS
    INTERNAL_AllocateVirtualPage
    (
        OUT         void**                      MemoryRegion,
        IN          const size_t                MappingLength,
        IN          const MemoryPermission      Access,
        IN          void*                       FixedAddress
    );

// -----------------------------------------------------------------------------
//  IMPLEMENTATION
// -----------------------------------------------------------------------------

static
int
    INTERNAL_GetPageProtection
    (
        IN          const MemoryPermission      Access
    )
{
    int prot = 0;

    if( Access & kRead )
    {
        prot |= PROT_READ;
    }
    if( Access & kWrite )
    {
        prot |= PROT_WRITE;
    }
    if( Access & kExecute )
    {
        prot |= PROT_EXEC;
    }

    return prot;
}

static
void
    INTERNAL_AllocatorInitialize
    (
        OUT         allocator_t*                Allocator,
        IN          uint8_t*                    VMPage,
        IN          uint32_t                    Capacity,
        IN          uint32_t                    Alignment
    )
{
    __NOT_NULL_RETURN_VOID( Allocator );

    Allocator->Buffer           = VMPage;
    Allocator->Capacity         = Capacity;
    Allocator->BuiltinAlignment = Alignment;
    Allocator->Size             = 0;
}

static
BWSR_STATUS
    INTERNAL_Allocator_GetNextOffsetInBuffer
    (
        OUT         uint8_t**                   Data,
        IN  OUT     allocator_t*                Allocator,
        IN          const uint32_t              BufferSize,
        IN          const uint32_t              Alignment
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uint32_t        alignment       = 0;
    uintptr_t       pageCeiling     = 0;

    __NOT_NULL( Data, Allocator );
    __GREATER_THAN_0( BufferSize );

    alignment   = ( ( Alignment != 0 )
                    ? Alignment
                    : Allocator->BuiltinAlignment );
    pageCeiling = (uintptr_t)( Allocator->Buffer + Allocator->Size );

    Allocator->Size += ( ALIGN_CEIL( pageCeiling, alignment ) - pageCeiling );

    if( ( Allocator->Size + BufferSize ) > Allocator->Capacity )
    {
        BWSR_DEBUG( LOG_WARNING, "Allocator not large enough!\n" );
        retVal = ERROR_MEMORY_OVERFLOW;
    }
    else {
        *Data = (uint8_t*)( Allocator->Buffer + Allocator->Size );
        Allocator->Size += BufferSize;

        retVal = ERROR_SUCCESS;
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_SetPagePermission
    (
        IN  OUT     void*                       Address,
        IN          const size_t                PageSize,
        IN          const MemoryPermission      Access
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    int             protection      = 0;

    __NOT_NULL( Address );
    __GREATER_THAN_0( PageSize );

    protection = INTERNAL_GetPageProtection( Access );

    if( 0 != mprotect( Address,
                       PageSize,
                       protection ) )
    {
        BWSR_DEBUG( LOG_ERROR, "mprotect() Failed\n" );
        retVal = ERROR_MEMORY_PERMISSION;
    }
    else {
        retVal = ERROR_SUCCESS;
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_AllocateVirtualPage
    (
        OUT         void**                      MemoryRegion,
        IN          const size_t                MappingLength,
        IN          const MemoryPermission      Access,
        IN          void*                       FixedAddress
    )
{
    BWSR_STATUS retVal      = ERROR_FAILURE;
    int         protection  = 0;
    int         flags       = ( MAP_PRIVATE | MAP_ANONYMOUS );

    __NOT_NULL( MemoryRegion );
    __GREATER_THAN_0( MappingLength );

    protection = INTERNAL_GetPageProtection( Access );

    if( NULL != FixedAddress )
    {
        flags |= MAP_FIXED;
    }

    if( MAP_FAILED == ( *MemoryRegion = mmap( FixedAddress,
                                              MappingLength,
                                              protection,
                                              flags,
                                              kMmapFd,
                                              kMmapFdOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "mmap() Failed\n" );
        retVal = ERROR_MEMORY_MAPPING;
    }
    else {
        retVal = ERROR_SUCCESS;
    } // mmap()

    return retVal;
}

BWSR_STATUS
    MemoryAllocator_AllocateExecutionBlock
    (
        IN  OUT     memory_range_t**            MemoryRange,
        IN  OUT     memory_allocator_t*         Allocator,
        IN          size_t                      BufferSize
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uint8_t*        result          = NULL;
    void*           page            = NULL;
    size_t          i               = 0;
    size_t          allocatorSize   = 0;
    size_t          pageSize        = 0;

    __NOT_NULL( Allocator, MemoryRange );
    __GREATER_THAN_0( BufferSize );

    pageSize = (size_t)sysconf( _SC_PAGESIZE );

    if( BufferSize > pageSize )
    {
        BWSR_DEBUG( LOG_ERROR,
                    "Requested Size is too large: %zu\n",
                    BufferSize );
        return ERROR_MEMORY_OVERFLOW;
    } // dummy check

    for( i = 0; ( i < Allocator->AllocatorCount ) && ( ERROR_SUCCESS != retVal ); i++ )
    {
        retVal = INTERNAL_Allocator_GetNextOffsetInBuffer( &result,
                                                           &Allocator->Allocators[ i ],
                                                           BufferSize,
                                                           0 );
    } // for()

    if( !result )
    {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_AllocateVirtualPage( &page,
                                                                      pageSize,
                                                                      kNoAccess,
                                                                      NULL ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_AllocateVirtualPage() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = INTERNAL_SetPagePermission( page,
                                                                        pageSize,
                                                                        kReadExecute ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "OSMemory_SetPermission() Failed\n" );
            }
            else {
                Allocator->AllocatorCount++;
                allocatorSize = Allocator->AllocatorCount * sizeof( allocator_t );

                if( NULL == Allocator->Allocators )
                {
                    Allocator->Allocators = (allocator_t*) BwsrMalloc( allocatorSize );
                }
                else {
                    Allocator->Allocators = (allocator_t*) BwsrRealloc( Allocator->Allocators, allocatorSize );
                }

                if( NULL == Allocator->Allocators )
                {
                    BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
                    retVal = ERROR_MEM_ALLOC;
                }
                else {
                    INTERNAL_AllocatorInitialize( &Allocator->Allocators[ Allocator->AllocatorCount - 1 ],
                                                  (uint8_t*) page,
                                                  (uint32_t) sysconf( _SC_PAGESIZE ),
                                                  8 );

                    retVal = INTERNAL_Allocator_GetNextOffsetInBuffer( &result,
                                                                       &Allocator->Allocators[ Allocator->AllocatorCount - 1 ],
                                                                       BufferSize,
                                                                       0 );
                } // BwsrMalloc()
            } // OSMemory_SetPermission()
        } // INTERNAL_AllocateVirtualPage()
    } // !result

    if( ERROR_SUCCESS == retVal )
    {
        if( NULL == ( *MemoryRange = (memory_range_t*) BwsrMalloc( sizeof( memory_range_t ) ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
            retVal = ERROR_MEM_ALLOC;
        }
        else {
            ( *MemoryRange )->Start = (uintptr_t)result;
            ( *MemoryRange )->Size  = BufferSize;
        } // BwsrMalloc()
    } // INTERNAL_Allocator_GetNextOffsetInBuffer()

    return retVal;
}