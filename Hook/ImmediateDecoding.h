
#ifndef __IMEDIATE_DECODING_H__
#define __IMEDIATE_DECODING_H__

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>
#include "utility/utility.h"

// -----------------------------------------------------------------------------
//  STRUCTURES
// -----------------------------------------------------------------------------

struct {
    // Destination register
    uint32_t    Destination     : 5;
    // Upper immediate `19` bits
    uint32_t    ImmHi           : 19;
    // Must be 10000 == 0x10
    uint32_t    Dummy_0         : 5;
    // Lower immediate `2` bits
    uint32_t    ImmLo           : 2;
    // `0` for `ADR`. `1` for `ADRP`.
    uint32_t    Op              : 1;
} InstructionDecoder;

// -----------------------------------------------------------------------------
//  IMMEDIATE DECODING
// -----------------------------------------------------------------------------

/**
 * \brief Performs a sign extension
 * \param[in]       Extension       Value by the sign extension performed.
 * \param[in]       SignBit         Bit position of the sign bit (0-indexed).
 * \return `int64_t` sign-extended value of `Extension`.
 */
static inline
int64_t
    INTERNAL_SignExtend
    (
        IN          int64_t         Extension,
        IN          int             SignBit
    )
{
    int64_t     sign_mask       = 0;
    int64_t     extend          = 0;

    sign_mask   = ( 0 - GET_BIT( Extension, ( SignBit - 1 ) ) );
    extend      = ( Extension | ( ( sign_mask >> SignBit ) << SignBit ) );

    return extend;
}

/**
 * \brief Extract and calculate a 26-bit immediate offset from the
 * given instruction.
 * \return `int64_t` the calculated 64-bit offset.
 */
static inline
int64_t
    Imm26Offset
    (
        IN          uint32_t        Instruction
    )
{
    int64_t     offset      = 0;
    int64_t     imm26       = 0;

    imm26   = GET_BITS( Instruction, 0, 25 );
    offset  = ( imm26 << 2 );
    offset  = INTERNAL_SignExtend( offset, 28 );

    return offset;
}

/**
 * \brief Extract and calculate a 19-bit immediate offset from the
 * given instruction.
 * \return `int64_t` the calculated 64-bit offset.
 */
static inline
int64_t
    Imm19Offset
    (
        IN          uint32_t        Instruction
    )
{
    int64_t     offset      = 0;
    int64_t     imm19       = 0;

    imm19   = GET_BITS( Instruction, 5, 23 );
    offset  = ( imm19 << 2 );
    offset  = INTERNAL_SignExtend( offset, 21 );

    return offset;
}

/**
 * \brief Extract and calculate a 14-bit immediate offset from the
 * given instruction.
 * \return `int64_t` the calculated 64-bit offset.
 */
static inline
int64_t
    Imm14Offset
    (
        IN          uint32_t        Instruction
    )
{
    int64_t     offset      = 0;
    int64_t     imm14       = 0;

    imm14   = GET_BITS( Instruction, 5, 18 );
    offset  = ( imm14 << 2 );
    offset  = INTERNAL_SignExtend( offset, 16 );

    return offset;
}

/**
 * \brief Extract and calculate a combined offset from separate
 * high and low immediate values in the given instruction.
 * \return `int64_t` the calculated 64-bit offset.
 */
static inline
int64_t
    ImmHiImmLoOffset
    (
        IN          uint32_t        Instruction
    )
{
    int64_t imm = 0;

    *(uint32_t*) &InstructionDecoder = Instruction;

    imm = InstructionDecoder.ImmLo + ( InstructionDecoder.ImmHi << 2 );
    imm = INTERNAL_SignExtend( imm, 21 );

    return imm;
}

/**
 * \brief Extract and calculate a combined offset and left-shift
 * it by 12 bits from separate high and low immediate values in
 * the given instruction.
 * \return `int64_t` the calculated 64-bit offset.
 */
static inline
int64_t
    ImmHiImmLoZero12Offset
    (
        IN          uint32_t        Instruction
    )
{
    int64_t imm = 0;

    imm = ImmHiImmLoOffset( Instruction );
    imm = ( imm << 12 );

    return imm;
}

#endif // __IMEDIATE_DECODING_H__