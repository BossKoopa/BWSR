
#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include "utility/debug.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <stdint.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
//  ENUMS
// -----------------------------------------------------------------------------

typedef enum InstructionFields {
    kRdShift                            = 0,
    kRnShift                            = 5,
    kRtShift                            = 0,
} InstructionFields;

typedef enum UnconditionalBranchToRegisterOp {
    UnconditionalBranchToRegisterFixed  = 0xD6000000,
    BR                                  = UnconditionalBranchToRegisterFixed | 0x001F0000,
    BLR                                 = UnconditionalBranchToRegisterFixed | 0x003F0000,
} UnconditionalBranchToRegisterOp;

typedef enum RegisterType {
    kRegister_32,
    kRegister_W                         = kRegister_32,
    kRegister_64,
    kRegister_X                         = kRegister_64,
    kRegister,

    kVRegister,
    kSIMD_FP_Register_8,
    kSIMD_FP_Register_B                 = kSIMD_FP_Register_8,
    kSIMD_FP_Register_16,
    kSIMD_FP_Register_H                 = kSIMD_FP_Register_16,
    kSIMD_FP_Register_32,
    kSIMD_FP_Register_S                 = kSIMD_FP_Register_32,
    kSIMD_FP_Register_64,
    kSIMD_FP_Register_D                 = kSIMD_FP_Register_64,
    kSIMD_FP_Register_128,
    kSIMD_FP_Register_Q                 = kSIMD_FP_Register_128,

    kInvalid
} RegisterType;

typedef enum AddSubImmediateOp {
    AddSubImmediateFixed                = 0x11000000,
    ADD_w_imm                           = AddSubImmediateFixed | ( 0b00 << 31 ) | ( 0b00 << 30 ),
    SUB_w_imm                           = AddSubImmediateFixed | ( 0b00 << 31 ) | ( 0b01 << 30 ),
    ADD_x_imm                           = AddSubImmediateFixed | ( 0b01 << 31 ) | ( 0b00 << 30 ),
    SUB_x_imm                           = AddSubImmediateFixed | ( 0b01 << 31 ) | ( 0b01 << 30 ),
} AddSubImmediateOp;

typedef enum LoadRegLiteralOp {
    LiteralLoadRegisterFixed            = 0x18000000,
    LiteralLoadRegisterFixedMask        = 0x3B000000,
    LDR_w_literal                       = LiteralLoadRegisterFixed | ( 0b00 << 30 ) | ( 0b00 << 26 ),
    LDR_x_literal                       = LiteralLoadRegisterFixed | ( 0b01 << 30 ) | ( 0b00 << 26 ),
    LDR_s_literal                       = LiteralLoadRegisterFixed | ( 0b00 << 30 ) | ( 0b01 << 26 ),
    LDR_d_literal                       = LiteralLoadRegisterFixed | ( 0b01 << 30 ) | ( 0b01 << 26 ),
    LDR_q_literal                       = LiteralLoadRegisterFixed | ( 0b10 << 30 ) | ( 0b01 << 26 ),
} LoadRegLiteralOp;

typedef enum Shift {
    NO_SHIFT                            = -1,
    LSL                                 = 0x0,
    LSR                                 = 0x1,
    ASR                                 = 0x2,
    ROR                                 = 0x3,
    MSL                                 = 0x4
} Shift;

typedef enum MoveWideImmediateOp {
    MoveWideImmediateFixed              = 0x12800000,
    MOVZ                                = 0x40000000,
    MOVK                                = 0x60000000,
} MoveWideImmediateOp;

// Load/store
typedef enum LoadStoreOp {
    STR_x                               = ( 0b11 << 30 ) | ( 0b00 << 22 ),
    LDR_x                               = ( 0b11 << 30 ) | ( 0b01 << 22 ),
} LoadStoreOp;

typedef enum AddrMode {
    AddrModeOffset,
    AddrModePreIndex,
    AddrModePostIndex
} AddrMode;

typedef enum LoadStoreUnsignedOffset {
    LoadStoreUnsignedOffsetFixed        = 0x39000000,
} LoadStoreUnsignedOffset;

typedef enum PCRelAddressingOp {
    PCRelAddressingFixed                = 0x10000000,
    PCRelAddressingFixedMask            = 0x1F000000,
    PCRelAddressingMask                 = 0x9F000000,
    ADR                                 = PCRelAddressingFixed | 0x00000000,
    ADRP                                = PCRelAddressingFixed | 0x80000000,
} PCRelAddressingOp;

typedef enum ReferenceLinkType {
    kLabelImm19
} ReferenceLinkType;

// -----------------------------------------------------------------------------
//  STRUCTURES
// -----------------------------------------------------------------------------

typedef struct memory_buffer_t {
    uint8_t*                Buffer;
    uint32_t                BufferSize;
    uint32_t                BufferCapacity;
} memory_buffer_t;

typedef struct register_data_t {
    int                     RegisterId;
    int                     RegisterSize;
    RegisterType            RegisterType;
} register_data_t;

typedef struct reference_instruct_t {
    int                     LinkType;
    size_t                  Offset;
} reference_instruct_t;

typedef struct relocation_data_t {
    size_t                  ReferenceInstructionCount;
    reference_instruct_t*   ReferenceInstructions;
    uintptr_t               PcOffset;
    uint8_t                 Data[ 8 ];
    uint8_t                 DataSize;
} relocation_data_t;

typedef struct operand_t {
    int64_t                 Immediate;
    register_data_t*        Register;
    Shift                   Shift;
    int32_t                 ShiftExtendImmediate;
} operand_t;

typedef struct assembler_t {
    uintptr_t               FixedAddress;
    uintptr_t               FixedMemoryRange;
    memory_buffer_t         Buffer;
    relocation_data_t**     RelocationData;
    size_t                  RelocationDataSize;
    size_t                  RelocationDataCapacity;
} assembler_t;

typedef struct memory_operand_t {
    register_data_t         Base;
    int64_t                 Offset;
    AddrMode                AddressMode;
} memory_operand_t;

// -----------------------------------------------------------------------------
//  EXPORTED FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * \brief Initializes an Assembler and allocates it it's buffer.
 * \param[in]           Assembler           A
 * \param[in]           FixedAddress        Start of the memory range.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Assembler` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if allocation of the `Assembler`'s buffer fails.
 * \retval `ERROR_SUCCESS` if `Assembler` was initialized and a buffer was created.
 * \warning This method allocates `Assembler->Buffer.Buffer`.
 */
BWSR_STATUS
    Assembler_Initialize
    (
        IN          assembler_t*                Assembler,
        IN          uintptr_t                   FixedAddress
    );

/**
 * \brief Allocates and initializes `RelocationData` and updates it's
 * contents with `Data`. This is then appended to the
 * `Assembler->RelocationData` array.
 * \param[in,out]       RelocationData      Stores `Data`.
 * \param[in]           Assembler           A
 * \param[in]           Data                Typically an address or label.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `RelocationData` or `Assembler` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if any memory allocation fails.
 * \retval `ERROR_SUCCESS` if `RelocationData` was allocated and initialized.
 * \warning This method allocates `RelocationData` and `Assembler->RelocationData`.
 */
BWSR_STATUS
    Assembler_CreateRelocationData
    (
        IN  OUT     relocation_data_t**         RelocationData,
        IN          assembler_t*                Assembler,
        IN          uint64_t                    Data
    );

/**
 * \brief Iterates through `Assembler->RelocationData` array, fixes up each
 * instruction, and then emits the instructions into `Assembler->Buffer`.
 * \param[in]           Assembler           A
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Assembler` is `NULL`.
 * \retval `ERROR_INVALID_ARGUMENT_VALUE` if `Assembler` contains relocation data with a size not greater than 0.
 * \retval `ERROR_MEM_ALLOC` if reallocation of `Assembler`'s buffer fails.
 * \retval `ERROR_SUCCESS` if `Assembler`'s buffer was updated.
 * \warning Through the call chain, `Assembler`'s buffer may be reallocated.
 */
BWSR_STATUS
    Assembler_WriteRelocationDataToPageBuffer
    (
        IN          assembler_t*                Assembler
    );

/**
 * \brief Writes a given `Value` as an instruction to `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Value               An instruction to emit into `Buffer`.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
BWSR_STATUS
    Assembler_Write32BitInstruction
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          uint32_t                    Value
    );

/**
 * \brief `LoadStore` instructions are used to load data from memory into
 * a register (Load) or store data from a register back into memory (Store).
 * These instructions are written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Op                  Expected to be either `STR` or `LDR`.
 * \param[in]           Register            Load and store operations Register.
 * \param[in]           Addr                Address mode, offset, and register id.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer`, `Result`, or `Addr` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
BWSR_STATUS
    Assembler_LoadStore
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          LoadStoreOp                 Op,
        IN          const register_data_t*      Register,
        IN          const memory_operand_t*     Addr
    );

/**
 * \brief `ADRP` (Address of Page) and `ADD` instructions are used together
 * to calculate the address of a large memory region. The `ADRP` instruction
 * computes the address of a 4KB page aligned with a specified memory address.
 * The `ADD` instruction is used to add an additional offset to the address
 * calculated by `ADRP`.
 * These instructions are written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Register            Where the result of an operation is stored or where data is moved to.
 * \param[in]           From                Start address.
 * \param[in]           To                  End address.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` or `Destination` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
BWSR_STATUS
    Assembler_ADRP_ADD
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    From,
        IN          uint64_t                    To
    );

/**
 * \brief `MOV` instruction is used to move (or copy) a value into a register.
 * This instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Register            Where the result of an operation is stored or where data is moved to.
 * \param[in]           Immediate           Immediate of encoded instruction.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer` or `Destination` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if the reallocation of `Buffer` fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
 */
BWSR_STATUS
    Assembler_MOV
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Register,
        IN          uint64_t                    Immediate
    );

/**
 * \brief `LDR` (Load Register) instruction is used to load data from memory
 * into a register.
 * This instruction is written into the provided `Buffer`.
 * \param[in,out]       Buffer              Buffer to emit instruction.
 * \param[in]           Result              Load and store instructions Register.
 * \param[in]           Relocation          Contains branching address or location of a specific label.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Buffer`, `Result`, or `Relocation` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if any allocation fails.
 * \retval `ERROR_SUCCESS` if `Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Buffer` may be reallocated.
*/
BWSR_STATUS
    Assembler_WriteInstruction_LDR
    (
        IN  OUT     memory_buffer_t*            Buffer,
        IN          register_data_t*            Result,
        IN          relocation_data_t*          Relocation
    );

/**
 * \brief `LDR` and `BR` instructions are used together to load a literal value
 * with an immediate offset and branch to a different part of the code or to a
 * specific label.
 * These instructions are written into the provided `Assembler->Buffer`.
 * \param[in,out]       Assembler           A
 * \param[in]           Address             Branching address or location of a specific label.
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Assembler` is `NULL`.
 * \retval `ERROR_MEM_ALLOC` if any allocation fails.
 * \retval `ERROR_SUCCESS` if `Assembler->Buffer` was updated with the encoded instruction.
 * \warning Through the call chain, `Assembler->Buffer` may be reallocated.
*/
BWSR_STATUS
    Assembler_LiteralLdrBranch
    (
        IN  OUT     assembler_t*                Assembler,
        IN          uint64_t                    Address
    );

/**
 * \brief Free all allocations held by the `Assembler` and resets all
 * references and data tracking variables.
 * \param[in,out]       Assembler           A
 * \return `BWSR_STATUS`
 * \retval `ERROR_ARGUMENT_IS_NULL` if `Assembler` is `NULL`.
 * \retval `ERROR_SUCCESS` after all allocations are released.
 */
BWSR_STATUS
    Assembler_Release
    (
        IN  OUT     assembler_t*                Assembler
    );

#endif // __ASSEMBLER_H__