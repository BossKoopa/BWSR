
// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>

#if defined( __APPLE__ )

    #if __has_feature( ptrauth_calls )
        #include <ptrauth.h>
    #endif

    #include <mach/mach.h>
    #include <mach/vm_page_size.h>
    #include <libkern/OSCacheControl.h>

    // Set to this task `mach_task_self()`
    #define MEM_PROT_TASK           mach_task_self(),

#else

    #include <unistd.h>
    #include <sys/mman.h>

    // Not used on Android/Linux
    #define MEM_PROT_TASK
    // Used for compatibility with OSX.
    // Set to `0`.
    #define KERN_SUCCESS 0

#endif

#include "Hook/InlineHook.h"
#include "Hook/Assembler.h"
#include "Hook/ImmediateDecoding.h"

#include "utility/utility.h"
#include "utility/error.h"

#include "Memory/MemoryTracker.h"
#include "Memory/MemoryAllocator.h"

// Binary Warfare & System Reconnaissance

// -----------------------------------------------------------------------------
//  DEFINITIONS
// -----------------------------------------------------------------------------

// `W` register of size `32`
#define W( RegisterID ) (register_data_t)   \
{                                           \
    .RegisterId     = RegisterID,           \
    .RegisterSize   = 32,                   \
    .RegisterType   = kRegister_32          \
}

// `X` register of size `64`
#define X( RegisterID ) (register_data_t)   \
{                                           \
    .RegisterId     = RegisterID,           \
    .RegisterSize   = 64,                   \
    .RegisterType   = kRegister_64          \
}

#define MEMOP_ADDR( ADDRESS_MODE ) (memory_operand_t)   \
{                                                       \
    .Base           = TMP_REG_0,                        \
    .Offset         = 0,                                \
    .AddressMode    = ADDRESS_MODE                      \
}

#define ARM64_TMP_REG_NDX_0 17

// -----------------------------------------------------------------------------
//  ENUMS
// -----------------------------------------------------------------------------

typedef enum UnconditionalBranchOp {
    UnconditionalBranchFixed            = 0x14000000,
    UnconditionalBranchFixedMask        = 0x7C000000,
    UnconditionalBranchMask             = 0xFC000000,
    B                                   = UnconditionalBranchFixed | 0x00000000,
    BL                                  = UnconditionalBranchFixed | 0x80000000,
} UnconditionalBranchOp;

typedef enum CompareBranchOp {
    CompareBranchFixed                  = 0x34000000,
    CompareBranchFixedMask              = 0x7E000000,
} CompareBranchOp;

typedef enum ConditionalBranchOp {
    ConditionalBranchFixed              = 0x54000000,
    ConditionalBranchFixedMask          = 0xFE000000,
    ConditionalBranchMask               = 0xFF000010,
} ConditionalBranchOp;

typedef enum TestBranchOp {
    TestBranchFixed                     = 0x36000000,
    TestBranchFixedMask                 = 0x7E000000,
} TestBranchOp;

// -----------------------------------------------------------------------------
//  STRUCTURES
// -----------------------------------------------------------------------------

typedef struct intercept_routing_t      intercept_routing_t;
typedef struct interceptor_tracker_t    interceptor_tracker_t;

typedef struct trampoline_t {
    memory_range_t              Buffer;
} trampoline_t;

typedef struct relocation_context_t {
    uintptr_t                   Cursor;
    memory_range_t*             BaseAddress;
} relocation_context_t;

typedef struct interceptor_entry_t {
    uintptr_t                   HookFunctionAddress;
    uintptr_t                   Address;
    memory_range_t              Patched;
    memory_range_t              Relocated;
    intercept_routing_t*        Routing;
    uint8_t*                    OriginalCode;
} interceptor_entry_t;

typedef struct intercept_routing_t {
    interceptor_entry_t*        InterceptEntry;
    trampoline_t*               Trampoline;
    uintptr_t                   HookFunction;

#if defined( __APPLE__ )
    __typeof( vm_protect )*     MemoryProtectFn;
#elif defined( __ANDROID__ ) || defined( __linux__ )
    __typeof( mprotect )*       MemoryProtectFn;
#endif

    CallBeforePageWrite         BeforePageWriteFn;
    CallAfterPageWrite          AfterPageWriteFn;

} intercept_routing_t;

typedef struct interceptor_tracker_t {
    interceptor_entry_t*        Entry;
    interceptor_tracker_t*      Next;
    interceptor_tracker_t*      Previous;
} interceptor_tracker_t;

// -----------------------------------------------------------------------------
//  GLOBALS
// -----------------------------------------------------------------------------

static const register_data_t    TMP_REG_0           = X( ARM64_TMP_REG_NDX_0 );

static memory_allocator_t       gMemoryAllocator    = { 0 };

static interceptor_tracker_t    gInterceptorTracker =
{
    .Entry      = NULL,
    .Next       = &gInterceptorTracker,
    .Previous   = &gInterceptorTracker
};

// -----------------------------------------------------------------------------
//  PROTOTYPES
// -----------------------------------------------------------------------------

/**
 * TODO: Add documentation
 */

static
BWSR_STATUS
    INTERNAL_ApplyCodePatch
    (
        IN          const intercept_routing_t*  Routing,
        IN          const void*                 Address,
        IN          const uint8_t*              Buffer,
        IN          const uint32_t              BufferSize
    );

static
BWSR_STATUS
    INTERNAL_BackupOriginalCode
    (
        IN  OUT     interceptor_entry_t*        Entry
    );

static
BWSR_STATUS
    INTERNAL_Trampoline_Initialize
    (
        IN  OUT     trampoline_t**              Trampoline,
        IN          const uintptr_t             From,
        IN          const uintptr_t             To
    );

static
uintptr_t
    INTERNAL_GetContextCursor
    (
        IN          const relocation_context_t* Context
    );

static
BWSR_STATUS
    INTERNAL_CodeBuilder_ApplyAssemblerPagePatch
    (
        IN          const intercept_routing_t*  Routing,
        IN  OUT     assembler_t*                Assembler,
        OUT         memory_range_t*             MemRange
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_UnconditionalBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_LiteralLoadRegisterFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADR
    (
        IN          const relocation_context_t* Context,
        OUT         assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADRP
    (
        IN          const relocation_context_t* Context,
        OUT         assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_ConditionalBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_CompareBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_TestBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    );

static
BWSR_STATUS
    INTERNAL_CodeBuilder_AssembleBuffer
    (
        IN  OUT     assembler_t*                Assembler,
        IN  OUT     relocation_context_t*       Context
    );

static
BWSR_STATUS
    INTERNAL_CodeBuilder_AssembleAndPatch
    (
        IN          const intercept_routing_t*  Routing,
        IN  OUT     memory_range_t*             BaseAddress,
        OUT         memory_range_t*             Relocated,
        IN          const bool                  Branch
    );

static
BWSR_STATUS
    INTERNAL_InterceptRouting_Initialize
    (
        OUT         intercept_routing_t**       Routing,
        IN          interceptor_entry_t*        Entry,
        IN          uintptr_t                   FakeFunction
    );

static
BWSR_STATUS
    INTERNAL_ApplyTrampolineCodePatch
    (
        IN  OUT     intercept_routing_t*        Routing
    );

static
BWSR_STATUS
    INTERNAL_GenerateRelocatedCode
    (
        IN  OUT     intercept_routing_t*        Routing
    );

static
BWSR_STATUS
    INTERNAL_GenerateTrampoline
    (
        IN  OUT     intercept_routing_t*        Routing
    );

static
BWSR_STATUS
    INTERNAL_BuildRoutingAndActivateHook
    (
        IN  OUT     intercept_routing_t*        Routing
    );

static
BWSR_STATUS
    INTERNAL_InterceptorTracker_Initialize
    (
        OUT         interceptor_tracker_t**     Tracker
    );

static
void
    INTERNAL_InterceptorTracker_Release
    (
        IN  OUT     interceptor_tracker_t*      Tracker
    );

static
BWSR_STATUS
    INTERNAL_InterceptorEntry_Initialize
    (
        OUT         interceptor_entry_t**       Entry
    );

static
BWSR_STATUS
    INTERNAL_SetMemoryProtectionFunction
    (
        OUT         uintptr_t*                  MemoryProtectFn
    );

// -----------------------------------------------------------------------------
//  IMPLEMENTATION
// -----------------------------------------------------------------------------

static
BWSR_STATUS
    INTERNAL_ApplyCodePatch
    (
        IN          const intercept_routing_t*  Routing,
        IN          const void*                 Address,
        IN          const uint8_t*              Buffer,
        IN          const uint32_t              BufferSize
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    uint32_t        pageBoundary        = 0;
    uint32_t        crossOverBoundary   = 0;
    uintptr_t       crossOverPage       = 0;
    int             kRet                = 0;

    __NOT_NULL( Routing, Address, Buffer );
    __GREATER_THAN_0( BufferSize );

#if defined( __APPLE__ )
    uintptr_t       patchPage           = ALIGN_FLOOR( Address, vm_page_size );
    uintptr_t       remapDestPage       = patchPage;
#elif defined( __ANDROID__ ) || defined( __linux__ )
    int             vm_page_size        = (int) sysconf( _SC_PAGESIZE );
    uintptr_t       patchPage           = ALIGN_FLOOR( Address, vm_page_size );
    void*           remapDestPage       = (void*) patchPage;
#endif

    if( ( (uintptr_t)Address + BufferSize ) > ( patchPage + vm_page_size ) )
    {
        pageBoundary = ( patchPage + vm_page_size - (uintptr_t)Address );

        if( ERROR_SUCCESS != ( retVal = INTERNAL_ApplyCodePatch( Routing,
                                                                 (void*) Address,
                                                                 Buffer,
                                                                 pageBoundary ) ) )
        {
            // Must exit at this point.
        }
        else {
            crossOverPage       = (uintptr_t)Address + pageBoundary;
            crossOverBoundary   = BufferSize - pageBoundary;

            retVal = INTERNAL_ApplyCodePatch( Routing,
                                              (void*) crossOverPage,
                                              Buffer + pageBoundary,
                                              crossOverBoundary );
        } // INTERNAL_ApplyCodePatch()
    } // Cross over boundary

    if( 0 == pageBoundary )
    {
        if( NULL != Routing->BeforePageWriteFn )
        {
            Routing->BeforePageWriteFn( (uintptr_t)remapDestPage );
        }

        if( KERN_SUCCESS != ( kRet = Routing->MemoryProtectFn( MEM_PROT_TASK
                                                               remapDestPage,
                                                               vm_page_size,
#if defined( __APPLE__ )
                                                               false,
                                                               ( VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY ) ) ) )
#elif defined( __ANDROID__ ) || defined( __linux__ )
                                                               ( PROT_READ | PROT_WRITE | PROT_EXEC ) ) ) )
#endif
        {
            BWSR_DEBUG( LOG_ERROR, "Routing->VMProtect() Failed\n" );
            retVal = ERROR_MEMORY_PERMISSION;
        }
        else {
            memcpy( (void*) ( patchPage + ( (uint64_t)Address - (uint64_t)remapDestPage ) ),
                    Buffer,
                    BufferSize );

            if( KERN_SUCCESS != ( kRet = Routing->MemoryProtectFn( MEM_PROT_TASK
                                                                   remapDestPage,
                                                                   vm_page_size,
#if defined( __APPLE__ )
                                                                   false,
                                                                   ( VM_PROT_READ | VM_PROT_EXECUTE ) ) ) )
#elif defined( __ANDROID__ ) || defined( __linux__ )
                                                                   ( PROT_READ | PROT_EXEC ) ) ) )
#endif
            {
                BWSR_DEBUG( LOG_ERROR, "Routing->VMProtect() Failed\n" );
                retVal = ERROR_MEMORY_PERMISSION;
            }
            else {

                if( NULL != Routing->AfterPageWriteFn )
                {
                    Routing->AfterPageWriteFn( (uintptr_t)remapDestPage );
                }

                retVal = ERROR_SUCCESS;
            } // Routing->MemoryProtectFn()
        } // Routing->MemoryProtectFn()
    } // pageBoundary

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_BackupOriginalCode
    (
        IN  OUT     interceptor_entry_t*        Entry
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    uint8_t*        originalCode        = NULL;
    uint32_t        trampolineSize      = 0;

    __NOT_NULL( Entry );

    originalCode   = (uint8_t*) Entry->Address;
    trampolineSize = Entry->Patched.Size;

    if( NULL == ( Entry->OriginalCode = (uint8_t*) BwsrMalloc( trampolineSize ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        memcpy( Entry->OriginalCode,
                originalCode,
                trampolineSize );

        retVal = ERROR_SUCCESS;
    } // BwsrMalloc()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Trampoline_Initialize
    (
        IN  OUT     trampoline_t**              Trampoline,
        IN          const uintptr_t             From,
        IN          const uintptr_t             To
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    assembler_t     assembler       = { 0 };
    uint64_t        distance        = 0;
    uint64_t        adrpRange       = 0;
    uint8_t*        buffer          = NULL;
    uint32_t        value           = 0;

    __NOT_NULL( Trampoline )
    __GREATER_THAN_0( From, To )

    if( ERROR_SUCCESS != ( retVal = Assembler_Initialize( &assembler, From ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_Initialize() Failed\n" );
    }
    else {
        distance    = llabs( (int64_t) ( From - To ) );
        adrpRange   = ( UINT32_MAX - 1 );

        if( distance < adrpRange )
        {
            if( ERROR_SUCCESS != ( retVal = Assembler_ADRP_ADD( &assembler.Buffer,
                                                                (register_data_t*) &TMP_REG_0,
                                                                From,
                                                                To ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "Assembler_ADRP_ADD() Failed\n" );
            }
            else {
                value  = ( BR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
                retVal = Assembler_Write32BitInstruction( &assembler.Buffer, value );
            } // Assembler_ADRP_ADD()
        }
        else {
            retVal = Assembler_LiteralLdrBranch( &assembler, (uint64_t)To );
        } // distance < adrp_range

        if( ERROR_SUCCESS == retVal )
        {
            if( ERROR_SUCCESS != ( retVal = Assembler_WriteRelocationDataToPageBuffer( &assembler ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "Assembler_WriteRelocationDataToPageBuffer() Failed\n" );
            }
            else {
                if( NULL == ( buffer = (uint8_t*) BwsrMalloc( assembler.Buffer.BufferSize ) ) )
                {
                    BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
                    retVal = ERROR_MEM_ALLOC;
                }
                else {
                    memcpy( buffer,
                            assembler.Buffer.Buffer,
                            assembler.Buffer.BufferSize );

                    if( NULL == ( *Trampoline = (trampoline_t*) BwsrMalloc( sizeof( trampoline_t ) ) ) )
                    {
                        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
                        BwsrFree( buffer );
                        retVal = ERROR_MEM_ALLOC;
                    }
                    else {
                        ( *Trampoline )->Buffer.Start = (uintptr_t)buffer;
                        ( *Trampoline )->Buffer.Size  = assembler.Buffer.BufferSize;
                    } // BwsrMalloc()
                } // BwsrMalloc()
            } // Assembler_WriteRelocationDataToPageBuffer()
        } // SUCCESS

        (void) Assembler_Release( &assembler );
    } // Assembler_Initialize()

    return retVal;
}

static
uintptr_t
    INTERNAL_GetContextCursor
    (
        IN          const relocation_context_t* Context
    )
{
    return (uintptr_t)( Context->BaseAddress->Start + ( Context->Cursor - Context->BaseAddress->Start ) );
}

static
BWSR_STATUS
    INTERNAL_CodeBuilder_ApplyAssemblerPagePatch
    (
        IN          const intercept_routing_t*  Routing,
        IN  OUT     assembler_t*                Assembler,
        OUT         memory_range_t*             MemoryRange
    )
{
    BWSR_STATUS         retVal      = ERROR_FAILURE;
    memory_range_t*     block       = NULL;

    __NOT_NULL( Routing,
                Assembler,
                MemoryRange )

    if( Assembler->FixedAddress )
    {
        retVal = ERROR_SUCCESS;
    }
    else {
        if( ERROR_SUCCESS != ( retVal = MemoryAllocator_AllocateExecutionBlock( &block,
                                                                                &gMemoryAllocator,
                                                                                Assembler->Buffer.BufferSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "MemoryAllocator_AllocateExecutionBlock() Failed\n" );
        }
        else {
            Assembler->FixedMemoryRange = (uintptr_t)block;
            Assembler->FixedAddress     = block->Start;
        } // MemoryAllocator_AllocateExecutionBlock()
    } // fixed_addr

    if( ERROR_SUCCESS == retVal )
    {
        BWSR_DEBUG( LOG_NOTICE, "Patching hooked function call into function address...\n" );

        if( ERROR_SUCCESS != ( retVal = INTERNAL_ApplyCodePatch( Routing,
                                                                 (void*) Assembler->FixedAddress,
                                                                 Assembler->Buffer.Buffer,
                                                                 Assembler->Buffer.BufferSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_ApplyCodePatch() Failed\n" );
        }
        else {
            MemoryRange->Start = Assembler->FixedAddress;
            MemoryRange->Size  = Assembler->Buffer.BufferSize;
        } // INTERNAL_ApplyCodePatch()
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_UnconditionalBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    uintptr_t               cursorOffset        = 0;
    relocation_data_t*      relocationData      = NULL;
    uint32_t                value               = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset = INTERNAL_GetContextCursor( Context ) + Imm26Offset( Instruction );

    if( ERROR_SUCCESS != ( retVal = Assembler_CreateRelocationData( &relocationData,
                                                                    Assembler,
                                                                    cursorOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_CreateRelocationData() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = Assembler_WriteInstruction_LDR( &Assembler->Buffer,
                                                                        (register_data_t*) &TMP_REG_0,
                                                                        relocationData ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "Assembler_WriteInstruction_LDR() Failed\n" );
        }
        else {
            if( BL == ( Instruction & UnconditionalBranchMask ) )
            {
                value = ( BLR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
            }
            else {
                value = ( BR  | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
            } // BLR or BL

            retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, value );
        } // Assembler_WriteInstruction_LDR()
    } // Assembler_CreateRelocationData()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_LiteralLoadRegisterFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uintptr_t       cursorOffset    = 0;
    char            opc             = 0;
    int             rt              = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset = INTERNAL_GetContextCursor( Context ) + Imm19Offset( Instruction );
    rt  = GET_BITS( Instruction, 0,  4  );
    opc = GET_BITS( Instruction, 30, 31 );

    if( ERROR_SUCCESS != ( retVal = Assembler_MOV( &Assembler->Buffer,
                                                   (register_data_t*) &TMP_REG_0,
                                                   cursorOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_MOV() Failed\n" );
    }
    else {
        /**/ if( 0b00 == opc )
        {
            retVal = Assembler_LoadStore( &Assembler->Buffer,
                                          LDR_x,
                                          &W( rt ),
                                          &MEMOP_ADDR( AddrModeOffset ) );
        }
        else if( 0b01 == opc )
        {
            retVal = Assembler_LoadStore( &Assembler->Buffer,
                                          LDR_x,
                                          &X( rt ),
                                          &MEMOP_ADDR( AddrModeOffset ) );
        }
        else {
            BWSR_DEBUG( LOG_WARNING,
                        "Unexpected opcode: %c\n",
                        opc );
            retVal = ERROR_UNIMPLEMENTED;
        } // opcode
    } // Assembler_MOV()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADR
    (
        IN          const relocation_context_t* Context,
        OUT         assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uintptr_t       cursorOffset    = 0;
    int             rd              = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset = INTERNAL_GetContextCursor( Context ) + ImmHiImmLoOffset( Instruction );
    rd = GET_BITS( Instruction, 0, 4 );

    retVal = Assembler_MOV( &Assembler->Buffer,
                            &X( rd ),
                            cursorOffset );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADRP
    (
        IN          const relocation_context_t* Context,
        OUT         assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uintptr_t       cursorOffset    = 0;
    int             rd              = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset = INTERNAL_GetContextCursor( Context ) + ImmHiImmLoZero12Offset( Instruction );
    cursorOffset = arm64_trunc_page( cursorOffset );
    rd = GET_BITS( Instruction, 0, 4 );

    retVal = Assembler_MOV( &Assembler->Buffer,
                            &X( rd ),
                            cursorOffset );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_ConditionalBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    uintptr_t               cursorOffset        = 0;
    uint32_t                instruction         = 0;
    char                    bitSetPos           = 0;
    relocation_data_t*      relocationData      = NULL;
    uint32_t                value               = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset    = INTERNAL_GetContextCursor( Context ) + Imm19Offset( Instruction );
    instruction     = Instruction;
    bitSetPos       = GET_BITS( Instruction, 0, 3 ) ^ 1;

    SET_BITS( instruction, 0, 3, bitSetPos );
    SET_BITS( instruction, 5, 23, 3 );

    if( ERROR_SUCCESS != ( retVal = Assembler_CreateRelocationData( &relocationData,
                                                                    Assembler,
                                                                    cursorOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_CreateRelocationData() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, instruction ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "Assembler_Write32BitInstruction() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = Assembler_WriteInstruction_LDR( &Assembler->Buffer,
                                                                            (register_data_t*) &TMP_REG_0,
                                                                            relocationData ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "Assembler_WriteInstruction_LDR() Failed\n" );
            }
            else {
                value  = ( BR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
                retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, value );
            } // Assembler_WriteInstruction_LDR()
        } // Assembler_Write32BitInstruction()
    } // Assembler_CreateRelocationData()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_CompareBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    uintptr_t               cursorOffset        = 0;
    uint32_t                instruction         = 0;
    char                    op                  = 0;
    relocation_data_t*      relocationData      = NULL;
    uint32_t                value               = 0;

    __NOT_NULL( Context, Assembler )
    __GREATER_THAN_0( Instruction )

    cursorOffset    = INTERNAL_GetContextCursor( Context ) + Imm19Offset( Instruction );
    instruction     = Instruction;
    op              = GET_BIT( Instruction, 24 ) ^ 1;

    SET_BIT( instruction, 24, op );
    SET_BITS( instruction, 5, 23, 3 );

    if( ERROR_SUCCESS != ( retVal = Assembler_CreateRelocationData( &relocationData,
                                                                    Assembler,
                                                                    cursorOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_CreateRelocationData() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, instruction ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "Assembler_Write32BitInstruction() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = Assembler_WriteInstruction_LDR( &Assembler->Buffer,
                                                                            (register_data_t*) &TMP_REG_0,
                                                                            relocationData ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "Assembler_WriteInstruction_LDR() Failed\n" );
            }
            else {
                value  = ( BR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
                retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, value );
            } // Assembler_WriteInstruction_LDR()
        } // Assembler_Write32BitInstruction()
    } // Assembler_CreateRelocationData()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_WriteToBuffer_TestBranchFixed
    (
        IN          const relocation_context_t* Context,
        IN  OUT     assembler_t*                Assembler,
        IN          const uint32_t              Instruction
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    uintptr_t               cursorOffset        = 0;
    uint32_t                instruction         = 0;
    char                    op                  = 0;
    relocation_data_t*      relocationData      = NULL;
    uint32_t                value               = 0;

    __NOT_NULL( Context, Assembler );
    __GREATER_THAN_0( Instruction );

    cursorOffset    = INTERNAL_GetContextCursor( Context ) + Imm14Offset( Instruction );
    instruction     = Instruction;
    op              = GET_BIT( Instruction, 24 ) ^ 1;

    SET_BIT( instruction, 24, op );
    SET_BITS( instruction, 5, 18, 3 );

    if( ERROR_SUCCESS != ( retVal = Assembler_CreateRelocationData( &relocationData,
                                                                    Assembler,
                                                                    cursorOffset ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_CreateRelocationData() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, instruction ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "Assembler_Write32BitInstruction() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = Assembler_WriteInstruction_LDR( &Assembler->Buffer,
                                                                            (register_data_t*) &TMP_REG_0,
                                                                            relocationData ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "Assembler_WriteInstruction_LDR() Failed\n" );
            }
            else {
                value  = ( BR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
                retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, value );
            } // Assembler_WriteInstruction_LDR()
        } // Assembler_Write32BitInstruction()
    } // Assembler_CreateRelocationData()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_CodeBuilder_AssembleBuffer
    (
        IN  OUT     assembler_t*                Assembler,
        IN  OUT     relocation_context_t*       Context
    )
{
    BWSR_STATUS     retVal          = ERROR_SUCCESS;
    uint32_t        instruction     = 0;

    __NOT_NULL( Assembler, Context );

    while( ( ( Context->Cursor - Context->BaseAddress->Start ) < Context->BaseAddress->Size ) &&
           ( ERROR_SUCCESS == retVal ) )
    {
        instruction = *(uint32_t *)INTERNAL_GetContextCursor( Context );

        if( UnconditionalBranchFixed == ( instruction & UnconditionalBranchFixedMask ) )
        {
            retVal = INTERNAL_WriteToBuffer_UnconditionalBranchFixed( Context,
                                                                      Assembler,
                                                                      instruction );
        }
        else if( LiteralLoadRegisterFixed == ( instruction & LiteralLoadRegisterFixedMask ) )
        {
            retVal = INTERNAL_WriteToBuffer_LiteralLoadRegisterFixed( Context,
                                                                      Assembler,
                                                                      instruction );
        }
        else if( ( PCRelAddressingFixed == ( instruction & PCRelAddressingFixedMask ) ) &&
                 ( ADR                  == ( instruction & PCRelAddressingMask      ) ) )
        {
            retVal = INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADR( Context,
                                                                      Assembler,
                                                                      instruction );
        }
        else if( ( PCRelAddressingFixed == ( instruction & PCRelAddressingFixedMask ) ) &&
                 ( ADRP                 == ( instruction & PCRelAddressingMask      ) ) )
        {
            retVal = INTERNAL_WriteToBuffer_PCRelAddressingFixed_ADRP( Context,
                                                                       Assembler,
                                                                       instruction );
        }
        else if( ConditionalBranchFixed == ( instruction & ConditionalBranchFixedMask ) )
        {
            retVal = INTERNAL_WriteToBuffer_ConditionalBranchFixed( Context,
                                                                    Assembler,
                                                                    instruction );
        }
        else if( CompareBranchFixed == ( instruction & CompareBranchFixedMask ) )
        {
            retVal = INTERNAL_WriteToBuffer_CompareBranchFixed( Context,
                                                                Assembler,
                                                                instruction );
        }
        else if( TestBranchFixed == ( instruction & TestBranchFixedMask ) )
        {
            retVal = INTERNAL_WriteToBuffer_TestBranchFixed( Context,
                                                             Assembler,
                                                             instruction );
        }
        else {
            retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, instruction );
        }

        Context->Cursor += sizeof( uint32_t );
    } // while()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_CodeBuilder_AssembleAndPatch
    (
        IN          const intercept_routing_t*  Routing,
        IN  OUT     memory_range_t*             BaseAddress,
        OUT         memory_range_t*             Relocated,
        IN          const bool                  Branch
    )
{
    BWSR_STATUS             retVal          = ERROR_FAILURE;
    assembler_t             assembler       = { 0 };
    relocation_context_t    context         = { 0 };

    __NOT_NULL( Routing,
                BaseAddress,
                Relocated )

    context.BaseAddress = BaseAddress;
    context.Cursor      = BaseAddress->Start;

    if( ERROR_SUCCESS != ( retVal = Assembler_Initialize( &assembler, 0 ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assembler_Initialize() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_CodeBuilder_AssembleBuffer( &assembler, &context ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_CodeBuilder_AssembleBuffer() Failed\n" );
        }
        else {
            BaseAddress->Size = ( context.Cursor - context.BaseAddress->Start );

            // TODO: if last instr is unlink branch, ignore it
            if( true == Branch )
            {
                retVal = Assembler_LiteralLdrBranch( &assembler, INTERNAL_GetContextCursor( &context ) );
            }

            if( ERROR_SUCCESS == retVal )
            {
                if( ERROR_SUCCESS != ( retVal = Assembler_WriteRelocationDataToPageBuffer( &assembler ) ) )
                {
                    BWSR_DEBUG( LOG_ERROR, "Assembler_WriteRelocationDataToPageBuffer() Failed\n" );
                }
                else {
                    retVal = INTERNAL_CodeBuilder_ApplyAssemblerPagePatch( Routing,
                                                                           &assembler,
                                                                           Relocated );
                } // Assembler_WriteRelocationDataToPageBuffer()
            } // Assembler_LiteralLdrBranch()
        } // INTERNAL_CodeBuilder_AssembleBuffer()

        (void) Assembler_Release( &assembler );
    } // Assembler_Initialize()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_InterceptRouting_Initialize
    (
        OUT         intercept_routing_t**       Routing,
        IN          interceptor_entry_t*        Entry,
        IN          uintptr_t                   FakeFunction
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Routing, Entry )
    __GREATER_THAN_0( FakeFunction )

    if( NULL == ( *Routing = (intercept_routing_t*) BwsrMalloc( sizeof( intercept_routing_t ) ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "malloc() Failed\n" );
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_SetMemoryProtectionFunction( (uintptr_t*)&( *Routing )->MemoryProtectFn ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_SetMemoryProtectionFunction() Failed\n" );
        }
        else {
            ( *Routing )->InterceptEntry     = Entry;
            ( *Routing )->Trampoline         = NULL;
            ( *Routing )->HookFunction       = FakeFunction;
        } // INTERNAL_SetMemoryProtectionFunction()
    } // BwsrMalloc()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ApplyTrampolineCodePatch
    (
        IN  OUT     intercept_routing_t*        Routing
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    size_t          trampolineSize      = 0;
    uintptr_t       bufferStart         = 0;

    __NOT_NULL( Routing );

    trampolineSize  = Routing->Trampoline->Buffer.Size;
    bufferStart     = Routing->Trampoline->Buffer.Start;

    BWSR_DEBUG( LOG_NOTICE, "Patching Trampoline into Intercept Address...\n" );

    retVal = INTERNAL_ApplyCodePatch( Routing,
                                      (void*) Routing->InterceptEntry->Address,
                                      (uint8_t*) bufferStart,
                                      trampolineSize );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_GenerateRelocatedCode
    (
        IN  OUT     intercept_routing_t*        Routing
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    size_t          trampolineSize      = 0;
    uintptr_t       bufferStart         = 0;

    __NOT_NULL( Routing );

    trampolineSize  = Routing->Trampoline->Buffer.Size;
    bufferStart     = Routing->Trampoline->Buffer.Start;

    if( bufferStart == 0 )
    {
        BWSR_DEBUG( LOG_ERROR, "Routing failed. Cannot continue\n" );
        retVal = ERROR_ROUTING_FAILURE;
    }
    else {
        Routing->InterceptEntry->Patched.Start      = Routing->InterceptEntry->Address;
        Routing->InterceptEntry->Patched.Size       = trampolineSize;
        Routing->InterceptEntry->Relocated.Start    = 0;
        Routing->InterceptEntry->Relocated.Size     = 0;

        retVal = INTERNAL_CodeBuilder_AssembleAndPatch( Routing,
                                                        &Routing->InterceptEntry->Patched,
                                                        &Routing->InterceptEntry->Relocated,
                                                        true );

        if( ( ERROR_SUCCESS == retVal                                  ) &&
            ( 0             == Routing->InterceptEntry->Relocated.Size ) )
        {
            BWSR_DEBUG( LOG_ERROR, "Routing failed. Cannot continue\n" );
            retVal = ERROR_ROUTING_FAILURE;
        }
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_GenerateTrampoline
    (
        IN  OUT     intercept_routing_t*        Routing
    )
{
    BWSR_STATUS         retVal      = ERROR_FAILURE;
    const uintptr_t     from        = Routing->InterceptEntry->Address;
    const uintptr_t     to          = Routing->HookFunction;

    __NOT_NULL( Routing );

    retVal = INTERNAL_Trampoline_Initialize( &Routing->Trampoline,
                                             from,
                                             to );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_BuildRoutingAndActivateHook
    (
        IN  OUT     intercept_routing_t*        Routing
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Routing );

    if( ERROR_SUCCESS != ( retVal = INTERNAL_GenerateTrampoline( Routing ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_GenerateTrampoline() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_GenerateRelocatedCode( Routing ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_GenerateRelocatedCode() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = INTERNAL_BackupOriginalCode( Routing->InterceptEntry ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "INTERNAL_BackupOriginalCode() Failed\n" );
            }
            else {
                retVal = INTERNAL_ApplyTrampolineCodePatch( Routing );
            } // INTERNAL_BackupOriginalCode()
        } // INTERNAL_GenerateRelocatedCode()
    } // INTERNAL_GenerateTrampoline()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_InterceptorTracker_Initialize
    (
        OUT         interceptor_tracker_t**     Tracker
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Tracker )

    if( NULL == ( *Tracker = (interceptor_tracker_t*) BwsrMalloc( sizeof( interceptor_tracker_t ) ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        if( NULL == ( ( *Tracker )->Entry = (interceptor_entry_t*) BwsrMalloc( sizeof( interceptor_entry_t ) ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
            retVal = ERROR_MEM_ALLOC;
            BwsrFree( *Tracker );
        }
        else {
            ( *Tracker )->Next                  = &gInterceptorTracker;
            ( *Tracker )->Previous              = gInterceptorTracker.Previous;
            gInterceptorTracker.Previous->Next  = *Tracker;
            gInterceptorTracker.Previous        = *Tracker;

            retVal = ERROR_SUCCESS;
        } // BwsrMalloc( InterceptorEntry )
    } // BwsrMalloc( InterceptorTracker )

    return retVal;
}

static
void
    INTERNAL_InterceptorTracker_Release
    (
        IN  OUT     interceptor_tracker_t*      Tracker
    )
{
    __NOT_NULL_RETURN_VOID( Tracker );

    if( NULL != Tracker->Entry )
    {
        if( NULL != Tracker->Entry->Routing )
        {
            if( NULL != Tracker->Entry->Routing->Trampoline )
            {
                if( Tracker->Entry->Routing->Trampoline->Buffer.Start )
                {
                    BwsrFree( (void*)Tracker->Entry->Routing->Trampoline->Buffer.Start );
                } // Tracker->Entry->Routing->Trampoline->Buffer.Start

                BwsrFree( Tracker->Entry->Routing->Trampoline );
                Tracker->Entry->Routing->Trampoline = NULL;
            } // NULL != Tracker->Entry->Routing->Trampoline

            BwsrFree( Tracker->Entry->Routing );
            Tracker->Entry->Routing = NULL;
        } // NULL != Tracker->Entry->Routing

        if( NULL != Tracker->Entry->OriginalCode )
        {
            BwsrFree( Tracker->Entry->OriginalCode );
            Tracker->Entry->OriginalCode = NULL;
        } // NULL != Tracker->Entry->OriginalCode

        BwsrFree( Tracker->Entry );
    } // NULL != Tracker->Entry

    Tracker->Previous->Next     = Tracker->Next;
    Tracker->Next->Previous     = Tracker->Previous;
    Tracker->Next               = NULL;
    Tracker->Previous           = NULL;

    BwsrFree( Tracker );
}

static
BWSR_STATUS
    INTERNAL_InterceptorEntry_Initialize
    (
        OUT         interceptor_entry_t**       Entry
    )
{
    BWSR_STATUS             retVal          = ERROR_FAILURE;
    interceptor_tracker_t*  tracker         = NULL;

    __NOT_NULL( Entry )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_InterceptorTracker_Initialize( &tracker ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_InterceptorTracker_Initialize() Failed\n" );
    }
    else {
        *Entry = tracker->Entry;
    } // INTERNAL_InterceptorTracker_Initialize()

    return retVal;
}

#include "SymbolResolve/Darwin/Macho.h"

static
BWSR_STATUS
    INTERNAL_SetMemoryProtectionFunction
    (
        OUT         uintptr_t*      MemoryProtectFn
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

#if defined( __APPLE__ )
    if( ERROR_SUCCESS != ( retVal = BWSR_ResolveSymbol( "vm_protect",
                                                        "dyld",
                                                        MemoryProtectFn ) ) )
    {
        retVal = BWSR_ResolveSymbol( "_vm_protect",
                                     "libsystem_kernel",
                                     MemoryProtectFn );
    } // BWSR_ResolveSymbol()
#else
    *MemoryProtectFn = (uintptr_t)mprotect;
    retVal = ERROR_SUCCESS;
#endif

    return retVal;
}

BWSR_API
BWSR_STATUS
    BWSR_InlineHook
    (
        IN          void*           Address,
        IN          void*           FakeFunction,
        IN  OUT     void**          Original,
        IN          void*           BeforePageWriteFn,
        IN          void*           AfterPageWriteFn
    )
{
    BWSR_STATUS             retVal      = ERROR_FAILURE;
    interceptor_entry_t*    entry       = NULL;
    intercept_routing_t*    routing     = NULL;

    __NOT_NULL( Address, FakeFunction )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_InterceptorEntry_Initialize( &entry ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_InterceptorEntry_Initialize() Failed\n" );
    }
    else {

#if __has_feature( ptrauth_calls )
        entry->Address              = (uintptr_t) ptrauth_strip( Address, ptrauth_key_asia );
        entry->HookFunctionAddress  = (uintptr_t) ptrauth_strip( FakeFunction, ptrauth_key_asia );
#else
        entry->Address              = (uintptr_t) Address;
        entry->HookFunctionAddress  = (uintptr_t) FakeFunction;
#endif

        if( ERROR_SUCCESS != ( retVal = INTERNAL_InterceptRouting_Initialize( &routing,
                                                                              entry,
                                                                              (uintptr_t) FakeFunction ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_InterceptRouting_Initialize() Failed\n" );
        }
        else {

            routing->AfterPageWriteFn  = (CallAfterPageWrite)AfterPageWriteFn;
            routing->BeforePageWriteFn = (CallBeforePageWrite)BeforePageWriteFn;

            if( ERROR_SUCCESS != ( retVal = INTERNAL_BuildRoutingAndActivateHook( routing ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "INTERNAL_BuildRoutingAndActivateHook() Failed\n" );
            }
            else {
                entry->Routing = routing;

                if( NULL != Original )
                {
                    *Original = (void*) entry->Relocated.Start;

#if __has_feature( ptrauth_calls )
                    *Original = (void*) ptrauth_strip( *Original, ptrauth_key_asia );
                    *Original = (void*) ptrauth_sign_unauthenticated( *Original, ptrauth_key_asia, 0 );
#endif

                }

                retVal = ERROR_SUCCESS;
            } // INTERNAL_BuildRoutingAndActivateHook()
        } // INTERNAL_InterceptRouting_Initialize()
    } // INTERNAL_InterceptorEntry_Initialize()

    __DEBUG_RETVAL( retVal );
    return retVal;
}

BWSR_API
BWSR_STATUS
    BWSR_DestroyHook
    (
        IN          void*           Address
    )
{
    BWSR_STATUS             retVal      = ERROR_FAILURE;
    interceptor_tracker_t*  tracker     = gInterceptorTracker.Next;

    __NOT_NULL( Address )

    retVal = ERROR_NOT_FOUND;

    while( ( tracker         != &gInterceptorTracker ) &&
           ( ERROR_NOT_FOUND == retVal               ) )
    {
        if( NULL != tracker->Entry )
        {
            if( tracker->Entry->Patched.Start == (uintptr_t)Address )
            {
                retVal = INTERNAL_ApplyCodePatch( tracker->Entry->Routing,
                                                  (void*) tracker->Entry->Patched.Start,
                                                  tracker->Entry->OriginalCode,
                                                  tracker->Entry->Patched.Size );

                INTERNAL_InterceptorTracker_Release( tracker );
            }
        }

        tracker = tracker->Next;
    } // while()

    if( gInterceptorTracker.Next == &gInterceptorTracker )
    {
        if( NULL != gMemoryAllocator.Allocators )
        {
            BwsrFree( gMemoryAllocator.Allocators );

            gMemoryAllocator.Allocators     = NULL;
            gMemoryAllocator.AllocatorCount = 0;
        }
    }

    return retVal;
}

BWSR_API
void
    BWSR_DestroyAllHooks
    (
        void
    )
{
    interceptor_tracker_t* tracker = gInterceptorTracker.Next;

    while( tracker != &gInterceptorTracker && tracker != NULL )
    {
        if( NULL != tracker->Entry )
        {
            (void) INTERNAL_ApplyCodePatch( tracker->Entry->Routing,
                                            (void*) tracker->Entry->Patched.Start,
                                            tracker->Entry->OriginalCode,
                                            tracker->Entry->Patched.Size );

            INTERNAL_InterceptorTracker_Release( tracker );
        } // tracker->Entry

        tracker = gInterceptorTracker.Next;
    } // while()

    if( NULL != gMemoryAllocator.Allocators )
    {
        BwsrFree( gMemoryAllocator.Allocators );

        gMemoryAllocator.Allocators     = NULL;
        gMemoryAllocator.AllocatorCount = 0;
    } // gMemoryAllocator.Allocators
}