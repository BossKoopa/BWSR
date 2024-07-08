
// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include "Hook/Assembler.h"
#include "Memory/MemoryTracker.h"

// -----------------------------------------------------------------------------
//  DEFINITIONS
// -----------------------------------------------------------------------------

// Destination Register - Register where the result of an operation is stored or where data is moved to.
#define Rd( rd )  ( rd->RegisterId << kRdShift  )
// Result Register - Load and store operations.
#define Rt( rt )  ( rt->RegisterId << kRtShift  )
// Source Register - Register from which data is read or used as an input for an operation.
#define Rn( rn )  ( rn->RegisterId << kRnShift  )

#define ARM64_TMP_REG_NDX_0 17

#define OPERAND_IMMEDIATE( IMMEDIATE ) (operand_t)                  \
{                                                                   \
    .Immediate              = IMMEDIATE,                            \
    .Register               = (register_data_t*) &InvalidRegister,  \
    .Shift                  = NO_SHIFT,                             \
    .ShiftExtendImmediate   = 0                                     \
}

// -----------------------------------------------------------------------------
//  GLOBALS
// -----------------------------------------------------------------------------

static const register_data_t InvalidRegister =
{
    .RegisterId     = 0,
    .RegisterSize   = 0,
    .RegisterType   = kInvalid
};

static const register_data_t TMP_REG_0 =
{
    .RegisterId     = ARM64_TMP_REG_NDX_0,
    .RegisterSize   = 64,
    .RegisterType   = kRegister_64
};

// -----------------------------------------------------------------------------
//  PROTOTYPES
// -----------------------------------------------------------------------------

/**
 * \brief Writes the given `InputBuffer` into `Buffer`.
 * \param[in,out]       Buffer              Memory buffer recieving `InputBuffer`
 * \param[in]           InputBuffer         Input buffer should be an Instruction.
 * \param[in]           InputBufferSize     The size of the instruction being written.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` or `InputBuffer` is `NULL`.
 * \retval `ERROR_INVALID_ARGUMENT_VALUE` if `InputBufferSize` is not greater than `0`.
 * \retval `ERROR_MEM_ALLOC` If reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with `InputBuffer`.
 * \warning `Buffer` may be reallocated if it cannot contain `InputBuffer`
 */
static
BWSR_STATUS
    INTERNAL_Assembler_WriteInstruction
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          uint8_t*                    InputBuffer,
        IN          int                         InputBufferSize
    );

/**
 * \brief Writes the encoded instruction for LDR into the provided `Buffer`
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Op                  LDR instruction.
 * \param[in]           Register            Load and store operations Register.
 * \param[in]           Immediate           Encoded Instruction.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` or `Result` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with `Immediate`.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_LoadRegisterLiteral
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          LoadRegLiteralOp            Op,
        IN          register_data_t*            Register,
        IN          int64_t                     Immediate
    );

/**
 * \brief `LDR` (Load Register) instruction is used to load data from memory
 * into a register.
 * This instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Register            Load and store instructions Register.
 * \param[in]           Immediate           Encoded Instruction.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` If `Buffer` or `Register` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` If the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with `Immediate`.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_LDR
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          int64_t                     Immediate
    );

/**
 * \brief `AddSubImmediate` refers to a specific type of instruction that allows
 * for conditional addition or subtraction of an immediate value to or from a register.
 * This instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Destination         Register where the result of an operation is stored or where data is moved to.
 * \param[in]           Source              Register from which data is read or used as an input for an operation.
 * \param[in]           Operand             Operand holding immediate value.
 * \param[in]           Op                  `ADD` or `SUB` literal fixed immediate in `32` or `64` bit.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer`, `Destination`, `Source`, or `Operand` is `NULL`
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded Instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_AddSubImmediate
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          const register_data_t*      Destination,
        IN          const register_data_t*      Source,
        IN          const operand_t*            Operand,
        IN          AddSubImmediateOp           Op
    );

/**
 * \brief `ADD` instructions are used to perform addition operations.
 * This `ADD` instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction
 * \param[in]           Destination         Register where the result of an operation is stored or where data is moved to.
 * \param[in]           Source              Register from which data is read or used as an input for an operation.
 * \param[in]           Immediate           Encoded instruction
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer`, `Destination`, or `Source` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded Instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_ADD
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          const register_data_t*      Destination,
        IN          const register_data_t*      Source,
        IN          int64_t                     Immediate
    );

/**
 * \brief Register operation size, 32 bits or 64 bits
 * \param[in]           Register            Register
 * \return `int32_t`
 * \retval `0` if register size is `32` bits.
 * \retval `INT32_MIN` if register size is `64` bits.
 */
static
int32_t
    INTERNAL_Assembler_OpEncode_SF
    (
        IN          const register_data_t*      Register
    );

/**
 * \brief `Move Wide` instruction allows for inserting a 16-bit immediate value
 * into a register at a specified bit position. This instruction is particularly
 * useful for constructing or modifying 64-bit values in `X` registers (64-bit)
 * or 32-bit values in `W` registers (32-bit).
 * This instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction
 * \param[in]           Register            Where the result of an operation is stored or where data is moved to.
 * \param[in]           Immediate           Encoded instruction
 * \param[in]           Shift               Shift of the encoded instructions. Expected as a multiple of `16`.
 * \param[in]           Op                  Move wide immediate fixed either `MOVZ` or `MOVK`
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` or `Destination` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_MoveWide
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    Immediate,
        IN          int                         Shift,
        IN          MoveWideImmediateOp         Op
    );

/**
 * \brief Resolves and updates references to instructions in `Buffer` based on `RelocationData`.
 * \param[in]           RelocationData      X
 * \param[in,out]       Buffer              B
 * \return `void`
 */
static
void
    INTERNAL_Assembler_LinkConfusedInstructions
    (
        IN          relocation_data_t*          RelocationData,
        IN  OUT     memory_buffer_t*            Buffer
    );

/**
 * \brief brief
 * \param[in]           Assembler           A
 * \param[in]           RelocationData      R
 * \return `void`
 */
static
void
    INTERNAL_Assembler_BindRelocationData
    (
        IN          assembler_t*                Assembler,
        IN          relocation_data_t*          RelocationData
    );

/**
 * \brief brief
 * \param[in,out]       RelocationData      R
 * \param[in]           LinkType            L
 * \param[in]           PCOffset            P
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `RelocationData` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if allocation of reference instruction fails.
 * \retval `ERROR_SUCCESS` if a reference instruction was added and updated.
 */
static
BWSR_STATUS
    INTERNAL_Assembler_LinkToOffset
    (
        IN  OUT     relocation_data_t*          RelocationData,
        IN          const int                   LinkType,
        IN          const size_t                PCOffset
    );

// -----------------------------------------------------------------------------
//  IMPLEMENTATION
// -----------------------------------------------------------------------------

static
BWSR_STATUS
    INTERNAL_Assembler_WriteInstruction
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          uint8_t*                    InputBuffer,
        IN          int                         InputBufferSize
    )
{
    BWSR_STATUS     retVal      = ERROR_SUCCESS;
    uint32_t        capacity    = 0;

    __NOT_NULL( Buffer, InputBuffer );
    __GREATER_THAN_0( InputBufferSize );

    if( ( Buffer->BufferSize + InputBufferSize ) > Buffer->BufferCapacity )
    {
        capacity = Buffer->BufferCapacity * 2;

        while( capacity < ( Buffer->BufferSize + InputBufferSize ) )
        {
            capacity *= 2;
        } // while()

        if( NULL == ( Buffer->Buffer = (uint8_t*) BwsrRealloc( Buffer->Buffer, capacity ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "BwsrRealloc() Failed\n" );
            Buffer->BufferSize = 0;
            retVal = ERROR_MEM_ALLOC;
        }
        else {
            Buffer->BufferCapacity = capacity;
        } // BwsrRealloc()
    } // Buffer->BufferCapacity

    if( ERROR_SUCCESS == retVal )
    {
        memcpy( ( Buffer->Buffer + Buffer->BufferSize ),
                InputBuffer,
                InputBufferSize );

        Buffer->BufferSize += InputBufferSize;
    } // ERROR_SUCCESS == retVal

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Assembler_LoadRegisterLiteral
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          LoadRegLiteralOp            Op,
        IN          register_data_t*            Register,
        IN          int64_t                     Immediate
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    uint32_t        encoding    = 0;

    __NOT_NULL( Buffer, Register )

    encoding    = ( Op
                    | BIT_SHIFT( ( Immediate >> 2 ), 26, 5 )
                    | Rt( Register ) );

    retVal  = Assembler_Write32BitInstruction( Buffer, encoding );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Assembler_LDR
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          int64_t                     Immediate
    )
{
    BWSR_STATUS         retVal      = ERROR_FAILURE;
    LoadRegLiteralOp    op          = LiteralLoadRegisterFixed;

    __NOT_NULL( Buffer, Register )

    switch( Register->RegisterType )
    {
        case kRegister_32:
        {
            op = LDR_w_literal;
            break;
        }

        case kRegister_X:
        {
            op = LDR_x_literal;
            break;
        }

        case kSIMD_FP_Register_S:
        {
            op = LDR_s_literal;
            break;
        }

        case kSIMD_FP_Register_D:
        {
            op = LDR_d_literal;
            break;
        }

        case kSIMD_FP_Register_Q:
        {
            op = LDR_q_literal;
            break;
        }

        default:
        {
            BWSR_DEBUG( LOG_WARNING, "This code should not be reachable!\n" );
            break;
        }
    }

    retVal = INTERNAL_Assembler_LoadRegisterLiteral( Buffer,
                                                     op,
                                                     Register,
                                                     Immediate );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Assembler_AddSubImmediate
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          const register_data_t*      Destination,
        IN          const register_data_t*      Source,
        IN          const operand_t*            Operand,
        IN          AddSubImmediateOp           Op
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    uint32_t        value       = 0;

    __NOT_NULL( Buffer,
                Destination,
                Source,
                Operand )

    if( 0 != Operand->Register->RegisterId )
    {
        retVal = ERROR_SUCCESS;
    }
    else {
        value   = ( Op
                    | Rd( Destination )
                    | Rn( Source )
                    | BIT_SHIFT( Operand->Immediate, 12, 10 ) );

        retVal  = Assembler_Write32BitInstruction( Buffer, value );
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Assembler_ADD
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          const register_data_t*      Destination,
        IN          const register_data_t*      Source,
        IN          int64_t                     Immediate
    )
{
    BWSR_STATUS         retVal      = ERROR_FAILURE;
    AddSubImmediateOp   op          = ADD_w_imm;

    __NOT_NULL( Buffer,
                Destination,
                Source )

    if( ( 64 == Destination->RegisterSize ) &&
        ( 64 == Source->RegisterSize      ) )
    {
        op = ADD_x_imm;
    }

    retVal = INTERNAL_Assembler_AddSubImmediate( Buffer,
                                                 Destination,
                                                 Source,
                                                 &OPERAND_IMMEDIATE( Immediate ),
                                                 op );

    return retVal;
}

static
int32_t
    INTERNAL_Assembler_OpEncode_SF
    (
        IN          const register_data_t*      Register
    )
{
    int32_t retVal = 0;

    __NOT_NULL_RETURN_0( Register )

    if( 64 == Register->RegisterSize )
    {
        retVal = INT32_MIN;
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_Assembler_MoveWide
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    Immediate,
        IN          int                         Shift,
        IN          MoveWideImmediateOp         Op
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    uint32_t        value       = 0;

    __NOT_NULL( Buffer, Register )

    if( 0 < Shift )
    {
        Shift /= 16;
    }
    else {
        Shift = 0;
    }

    value   = ( MoveWideImmediateFixed
                | Op
                | INTERNAL_Assembler_OpEncode_SF( Register )
                | BIT_SHIFT( Shift,     2,  21 )
                | BIT_SHIFT( Immediate, 16,  5 )
                | Rd( Register ) );

    retVal = Assembler_Write32BitInstruction( Buffer, value );

    return retVal;
}

static
void
    INTERNAL_Assembler_LinkConfusedInstructions
    (
        IN          relocation_data_t*          RelocationData,
        IN  OUT     memory_buffer_t*            Buffer
    )
{
    int64_t     fixupOffset     = 0;
    uint32_t    instruction     = 0;
    uint32_t    newInstruction  = 0;
    size_t      i               = 0;

    __NOT_NULL_RETURN_VOID( RelocationData, Buffer )

    for( i = 0; i < RelocationData->ReferenceInstructionCount; ++i )
    {
        fixupOffset     = RelocationData->PcOffset - RelocationData->ReferenceInstructions[ i ].Offset;
        instruction     = *(int32_t*)( Buffer->Buffer + RelocationData->ReferenceInstructions[ i ].Offset );
        newInstruction  = 0;

        if( kLabelImm19 == RelocationData->ReferenceInstructions[ i ].LinkType )
        {
            SET_BITS( instruction,
                      5,
                      23,
                      GET_BITS( ( fixupOffset >> 2 ), 0, 18 ) );

            newInstruction = instruction;
        }

        *(int32_t*)( Buffer->Buffer + RelocationData->ReferenceInstructions[ i ].Offset ) = newInstruction;
    } // for()
}

static
void
    INTERNAL_Assembler_BindRelocationData
    (
        IN          assembler_t*                Assembler,
        IN          relocation_data_t*          RelocationData
    )
{
    __NOT_NULL_RETURN_VOID( Assembler, RelocationData );

    RelocationData->PcOffset = Assembler->Buffer.BufferSize;

    if( 0 != RelocationData->ReferenceInstructionCount )
    {
        INTERNAL_Assembler_LinkConfusedInstructions( RelocationData, &Assembler->Buffer );
    }
}

static
BWSR_STATUS
    INTERNAL_Assembler_LinkToOffset
    (
        IN  OUT     relocation_data_t*          RelocationData,
        IN          const int                   LinkType,
        IN          const size_t                PCOffset
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    size_t          refSize     = 0;

    __NOT_NULL( RelocationData )

    RelocationData->ReferenceInstructionCount++;
    refSize = RelocationData->ReferenceInstructionCount * sizeof( reference_instruct_t );

    if( NULL == RelocationData->ReferenceInstructions )
    {
        RelocationData->ReferenceInstructions = (reference_instruct_t*) BwsrMalloc( refSize );
    }
    else {
        RelocationData->ReferenceInstructions = (reference_instruct_t*) BwsrRealloc( RelocationData->ReferenceInstructions, refSize );
    } // RelocationData->ReferenceInstructions

    if( NULL == RelocationData->ReferenceInstructions )
    {
        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
        RelocationData->ReferenceInstructionCount = 0;
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        RelocationData->ReferenceInstructions[ RelocationData->ReferenceInstructionCount - 1 ].LinkType = LinkType;
        RelocationData->ReferenceInstructions[ RelocationData->ReferenceInstructionCount - 1 ].Offset   = PCOffset;
        retVal = ERROR_SUCCESS;
    } // RelocationData->ReferenceInstructions

    return retVal;
}

BWSR_STATUS
    Assembler_Initialize
    (
        IN          assembler_t*                Assembler,
        IN          uintptr_t                   FixedAddress
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Assembler )

    if( NULL == ( Assembler->Buffer.Buffer = (uint8_t*) BwsrMalloc( 64 ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        Assembler->FixedAddress             = FixedAddress;
        Assembler->Buffer.BufferSize        = 0;
        Assembler->Buffer.BufferCapacity    = 64;
        Assembler->RelocationData           = NULL;
        Assembler->RelocationDataSize       = 0;
        Assembler->RelocationDataCapacity   = 0;

        retVal = ERROR_SUCCESS;
    } // BwsrMalloc()

    return retVal;
}

BWSR_STATUS
    Assembler_CreateRelocationData
    (
        IN  OUT     relocation_data_t**         Relocation,
        IN          assembler_t*                Assembler,
        IN          uint64_t                    Data
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    relocation_data_t**     relocationData      = NULL;
    size_t                  capacity            = 0;
    size_t                  refSize             = 0;

    __NOT_NULL( Relocation, Assembler )
    __GREATER_THAN_0( Data )

    if( NULL == ( *Relocation = (relocation_data_t*) BwsrMalloc( sizeof( relocation_data_t ) ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
        retVal = ERROR_MEM_ALLOC;
    }
    else {
        memcpy( ( *Relocation )->Data,
                &Data,
                sizeof( uint64_t ) );

        if( Assembler->RelocationDataSize >= Assembler->RelocationDataCapacity )
        {
            capacity = ( ( Assembler->RelocationDataCapacity == 0 )
                        ? 1
                        : ( Assembler->RelocationDataCapacity * 2 ) );
            refSize  = ( capacity * sizeof( relocation_data_t* ) );

            if( NULL == Assembler->RelocationData )
            {
                relocationData = (relocation_data_t**) BwsrMalloc( refSize );
            }
            else {
                relocationData = (relocation_data_t**) BwsrRealloc( Assembler->RelocationData, refSize );
            } // NULL == Assembler->RelocationData

            if( NULL == relocationData )
            {
                BwsrFree( *Relocation );
                capacity = 0;
                retVal = ERROR_MEM_ALLOC;
            }
            else {
                retVal = ERROR_SUCCESS;
            } // NULL == relocationData

            Assembler->RelocationData           = relocationData;
            Assembler->RelocationDataCapacity   = capacity;
        } // Assembler->RelocationDataSize

        if( ERROR_SUCCESS == retVal )
        {
            ( *Relocation )->DataSize                   = (uint8_t) sizeof( uint64_t );
            ( *Relocation )->ReferenceInstructionCount  = 0;
            ( *Relocation )->PcOffset                   = 0;
            ( *Relocation )->ReferenceInstructions      = NULL;

            Assembler->RelocationData[ Assembler->RelocationDataSize++ ] = *Relocation;
        }
    } // BwsrMalloc()

    return retVal;
}

BWSR_STATUS
    Assembler_WriteRelocationDataToPageBuffer
    (
        IN          assembler_t*                Assembler
    )
{
    BWSR_STATUS     retVal      = ERROR_SUCCESS;
    size_t          i           = 0;

    __NOT_NULL( Assembler )

    for( i = 0; ( i < Assembler->RelocationDataSize ) && ( ERROR_SUCCESS == retVal ); i++ )
    {
        INTERNAL_Assembler_BindRelocationData( Assembler, Assembler->RelocationData[ i ] );

        retVal = INTERNAL_Assembler_WriteInstruction( &Assembler->Buffer,
                                                      Assembler->RelocationData[ i ]->Data,
                                                      Assembler->RelocationData[ i ]->DataSize );
    } // for()

    return retVal;
}

BWSR_STATUS
    Assembler_Write32BitInstruction
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          uint32_t                    Value
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Buffer )

    retVal = INTERNAL_Assembler_WriteInstruction( Buffer,
                                                  (uint8_t*) &Value,
                                                  sizeof( uint32_t ) );

    return retVal;
}


BWSR_STATUS
    Assembler_LoadStore
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          LoadStoreOp                 Op,
        IN          const register_data_t*      Register,
        IN          const memory_operand_t*     Addr
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    int             scale       = 0;
    uint32_t        value       = 0;

    __NOT_NULL( Buffer,
                Register,
                Addr )

    if( AddrModeOffset != Addr->AddressMode )
    {
        retVal = ERROR_SUCCESS;
    }
    else {

        if( LoadStoreUnsignedOffsetFixed == ( Op & LoadStoreUnsignedOffsetFixed ) )
        {
            scale = GET_BITS( Op, 30, 31 );
        }

        value   = ( LoadStoreUnsignedOffsetFixed
                    | Op
                    | BIT_SHIFT( (int64_t)( Addr->Offset >> scale ), 12, 10 )
                    | ( Addr->Base.RegisterId << kRnShift )
                    | Rt( Register ) );
        retVal  = Assembler_Write32BitInstruction( Buffer, value );
    } // AddrModeOffset != Addr->AddressMode

    return retVal;
}

BWSR_STATUS
    Assembler_ADRP_ADD
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    From,
        IN          uint64_t                    To
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    uint64_t        from_PAGE       = ALIGN_FLOOR( From, 0x1000 );
    uint64_t        to_PAGE         = ALIGN_FLOOR( To,   0x1000 );
    uint64_t        to_PAGEOFF      = (uint64_t) ( To  % 0x1000 );
    uint32_t        value           = 0;

    __NOT_NULL( Buffer, Register )
    __GREATER_THAN_0( From, To )

    value   = ( ADRP
                | Rd( Register )
                | BIT_SHIFT( GET_BITS( ( to_PAGE - from_PAGE ) >> 12, 0, 1  ), 2,  29 )
                | BIT_SHIFT( GET_BITS( ( to_PAGE - from_PAGE ) >> 12, 2, 20 ), 19,  5 ) );

    if( ERROR_SUCCESS != ( retVal = Assembler_Write32BitInstruction( Buffer, value ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "Assemble_adrp() Failed\n" );
    }
    else {
        retVal = INTERNAL_Assembler_ADD( Buffer,
                                         Register,
                                         Register,
                                         to_PAGEOFF );
    } // Assemble_adrp()

    return retVal;
}

BWSR_STATUS
    Assembler_MOV
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    Immediate
    )
{
    BWSR_STATUS     retVal  = ERROR_FAILURE;

    const uint32_t  w0      = (uint32_t)( Immediate &  0xffffffff );
    const uint32_t  w1      = (uint32_t)( Immediate >> 32         );
    const uint16_t  h0      = (uint16_t)( w0        &  0xffff     );
    const uint16_t  h1      = (uint16_t)( w0        >> 16         );
    const uint16_t  h2      = (uint16_t)( w1        &  0xffff     );
    const uint16_t  h3      = (uint16_t)( w1        >> 16         );

    __NOT_NULL( Buffer, Register )
    __GREATER_THAN_0( Immediate )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_Assembler_MoveWide( Buffer,
                                                                 Register,
                                                                 h0,
                                                                 0,
                                                                 MOVZ ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_Assembler_MoveWide() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_Assembler_MoveWide( Buffer,
                                                                     Register,
                                                                     h1,
                                                                     16,
                                                                     MOVK ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_Assembler_MoveWide() Failed\n" );
        }
        else {
            if( ERROR_SUCCESS != ( retVal = INTERNAL_Assembler_MoveWide( Buffer,
                                                                         Register,
                                                                         h2,
                                                                         32,
                                                                         MOVK ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "INTERNAL_Assembler_MoveWide() Failed\n" );
            }
            else {
                retVal = INTERNAL_Assembler_MoveWide( Buffer,
                                                      Register,
                                                      h3,
                                                      48,
                                                      MOVK );
            } // Assemble_movk()
        } // Assemble_movk()
    } // Assemble_movz()

    return retVal;
}

BWSR_STATUS
    Assembler_WriteInstruction_LDR
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          relocation_data_t*          RelocationData
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Buffer,
                Register,
                RelocationData )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_Assembler_LinkToOffset( RelocationData,
                                                                     kLabelImm19,
                                                                     Buffer->BufferSize ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_Assembler_LinkToOffset() Failed\n" );
    }
    else {
        retVal = INTERNAL_Assembler_LDR( Buffer,
                                         Register,
                                         0 );
    } // INTERNAL_Assembler_LinkToOffset()

    return retVal;
}

BWSR_STATUS
    Assembler_LiteralLdrBranch
    (
        IN  OUT     assembler_t*                Assembler,
        IN          uint64_t                    Address
    )
{
    BWSR_STATUS             retVal              = ERROR_FAILURE;
    relocation_data_t*      relocationData      = NULL;
    uint32_t                value               = 0;

    __NOT_NULL( Assembler )
    __GREATER_THAN_0( Address )

    if( ERROR_SUCCESS != ( retVal = Assembler_CreateRelocationData( &relocationData,
                                                                    Assembler,
                                                                    Address ) ) )
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
            value  = ( BR | ( ARM64_TMP_REG_NDX_0 << kRnShift ) );
            retVal = Assembler_Write32BitInstruction( &Assembler->Buffer, value );
        } // Assembler_WriteInstruction_LDR()
    } // Assembler_CreateRelocationData()

    if( ERROR_SUCCESS != retVal )
    {
        if( NULL != relocationData )
        {
            BwsrFree( relocationData );
        }
    } // ERROR_SUCCESS != retVal

    return retVal;
}

BWSR_STATUS
    Assembler_Release
    (
        IN  OUT     assembler_t*                Assembler
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( Assembler )

    if( Assembler->RelocationData )
    {
        while( Assembler->RelocationDataSize-- )
        {
            BwsrFree( Assembler->RelocationData[ Assembler->RelocationDataSize ]->ReferenceInstructions );
            BwsrFree( Assembler->RelocationData[ Assembler->RelocationDataSize ] );
        } // while()

        BwsrFree( Assembler->RelocationData );
        Assembler->RelocationData = NULL;
    } // Assembler->RelocationData

    if( Assembler->Buffer.Buffer )
    {
        BwsrFree( Assembler->Buffer.Buffer );
        Assembler->Buffer.Buffer = NULL;
    } // Assembler->Buffer.Buffer

    if( Assembler->FixedMemoryRange )
    {
        BwsrFree( (void*)Assembler->FixedMemoryRange );
        Assembler->FixedMemoryRange = 0;
    } // Assembler->FixedMemoryRange

    retVal = ERROR_SUCCESS;

    return retVal;
}