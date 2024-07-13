
#ifndef __UTILITY_H__
#define __UTILITY_H__

// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include "utility/debug.h"
#include "utility/error.h"

// -----------------------------------------------------------------------------
//  BASE
// -----------------------------------------------------------------------------

#define IN
#define OUT
#define OPTIONAL

#define BWSR_API __attribute__( ( visibility( "default" ) ) )

#define ARRAY_LENGTH( ARRAY ) ( sizeof( ARRAY ) / sizeof( ARRAY[ 0 ] ) )

// -----------------------------------------------------------------------------
//  BIT MANIPULATION
// -----------------------------------------------------------------------------

// Truncate address to nearest lower multiple of page size `0x1000`.
#define arm64_trunc_page( x ) \
    ( ( x ) & ( ~( 0x1000 - 1 ) ) )

// Align page to boundary
#define ALIGN_FLOOR( ADDRESS, RANGE ) \
    ( (uintptr_t) ADDRESS & ~( (uintptr_t) RANGE - 1 ) )

// Left shift `Bits` after masking with `BitMaskShift` least significant bits,
// then shift left by `BitShift` positions.
#define BIT_SHIFT( Bits, BitMaskShift, BitShift ) \
    ( ( ( Bits ) & ( ( 1 << ( BitMaskShift ) ) - 1 ) ) << ( BitShift ) )

// Generate a bitmask with `BitShift` bits set to `1`.
#define GENERATE_BIT_MASK( BitShift ) \
    ( ( 1L << ( ( BitShift ) + 1 ) ) - 1 )

// Extract bits from `Bits` starting at position `StartBit` to `EndBit`.
#define GET_BITS( Bits, StartBit, EndBit ) \
    ( ( ( Bits ) >> ( StartBit ) ) & GENERATE_BIT_MASK( ( EndBit ) - ( StartBit ) ) )

// Extract a single bit from `Bits` at position `BitPos`.
#define GET_BIT( Bits, BitPos ) \
    ( ( ( Bits ) >> ( BitPos ) ) & 1 )

// Set a specific bit in `Bits` at position `BitPos` to `Bit` (0 or 1).
#define SET_BIT( Bits, BitPos, Bit ) \
    ( Bits = ( ( ( ~( 1 << ( BitPos ) ) ) & ( Bits ) ) | ( ( Bit ) << ( BitPos ) ) ) )

// Set bits in `Bits` from position `StartBit` to `EndBit` with bits
// from `ReplacementBits`.
#define SET_BITS( Bits, StartBit, EndBit, ReplacementBits ) \
    ( Bits = ( ( ( ~( GENERATE_BIT_MASK( ( EndBit ) - ( StartBit ) ) << ( StartBit ) ) ) & ( Bits ) ) \
                | ( ( ReplacementBits ) << ( StartBit ) ) ) )

// -----------------------------------------------------------------------------
//  MACRO EXPANSIONS && BASE CONDITIONS
// -----------------------------------------------------------------------------

// Bypass compiler warnings about unused parameters.
#define UNUSED_PARAMETER( Parameter ) (void) Parameter;

// Avoids return value by forcing `return;`
#define VOID_RETURN


// Checks if `Parameter` is `NULL`.
// If so, it returns `RETURN_VALUE`.
#define IF_PARAMETER_NULL_RETURN( RETURN_VALUE, Parameter )         \
    if( NULL == Parameter )                                         \
    {                                                               \
        BWSR_DEBUG( LOG_ERROR, #Parameter " is NULL\n" );           \
        return RETURN_VALUE;                                        \
    }

// Wraps `IF_PARAMETER_NULL_RETURN`
// Returns `ERROR_INVALID_ARGUMENT` if `Parameter` is `NULL`
#define IF_PARAMETER_NULL_RETURN_BWSR( Parameter ) \
    IF_PARAMETER_NULL_RETURN( ERROR_ARGUMENT_IS_NULL, Parameter )

// Wraps `IF_PARAMETER_NULL_RETURN`
// Returns `NULL` if `Parameter` is `NULL`
#define IF_PARAMETER_NULL_RETURN_NULL( Parameter ) \
    IF_PARAMETER_NULL_RETURN( NULL, Parameter )

// Wraps `IF_PARAMETER_NULL_RETURN`
// Returns `VOID` if `Parameter` is `NULL`
#define IF_PARAMETER_NULL_RETURN_VOID( Parameter ) \
    IF_PARAMETER_NULL_RETURN( VOID_RETURN, Parameter )

// Wraps `IF_PARAMETER_NULL_RETURN`
// Returns `0` if `Parameter` is `NULL`
#define IF_PARAMETER_NULL_RETURN_0( Parameter ) \
    IF_PARAMETER_NULL_RETURN( 0, Parameter )



// Checks if `Parameter` is less than or equal to 0.
// If so, it returns `RETURN_VALUE`.
#define IF_PARAMATER_NOT_GREATER_THAN_0_RETURN( RETURN_VALUE, Parameter )   \
    if( 0 >= Parameter )                                                    \
    {                                                                       \
        BWSR_DEBUG( LOG_ERROR, #Parameter " is less than 0!\n" );           \
        return RETURN_VALUE;                                                \
    }

// Wraps `IF_PARAMATER_NOT_GREATER_THAN_0_RETURN`
// Returns `ERROR_INVALID_ARGUMENT` if `Parameter` is less than or equal to 0.
#define IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_BWSR( Parameter ) \
    IF_PARAMATER_NOT_GREATER_THAN_0_RETURN( ERROR_INVALID_ARGUMENT_VALUE, Parameter )

// Wraps `IF_PARAMATER_NOT_GREATER_THAN_0_RETURN`
// Returns `NULL` if `Parameter` is less than or equal to 0.
#define IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_NULL( Parameter ) \
    IF_PARAMATER_NOT_GREATER_THAN_0_RETURN( NULL, Parameter )

// Wraps `IF_PARAMATER_NOT_GREATER_THAN_0_RETURN`
// Returns `VOID` if `Parameter` is less than or equal to 0.
#define IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_VOID( Parameter ) \
    IF_PARAMATER_NOT_GREATER_THAN_0_RETURN( VOID_RETURN, Parameter )



// Should be an impossible case
#define MACRO_EXPAND__0__

// Should be an impossible case
#define MACRO_EXPAND__1__( Parameter )

#define MACRO_EXPAND__2__( MACRO_EXPANSION, Parameter )         \
    MACRO_EXPANSION( Parameter )

#define MACRO_EXPAND__3__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__2__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__4__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__3__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__5__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__4__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__6__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__5__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__7__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__6__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__8__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__7__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__9__( MACRO_EXPANSION, Parameter, ... )    \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__8__( MACRO_EXPANSION, __VA_ARGS__ )

#define MACRO_EXPAND__10__( MACRO_EXPANSION, Parameter, ... )   \
    MACRO_EXPANSION( Parameter )                                \
    MACRO_EXPAND__9__( MACRO_EXPANSION, __VA_ARGS__ )

#define N_ARGS( ... )               \
    N_ARGS_HELPER1( __VA_ARGS__,    \
                    __10__,         \
                    __9__,          \
                    __8__,          \
                    __7__,          \
                    __6__,          \
                    __5__,          \
                    __4__,          \
                    __3__,          \
                    __2__,          \
                    __1__,          \
                    __0__ )

#define N_ARGS_HELPER1( ... )       \
    N_ARGS_HELPER2( __VA_ARGS__ )

#define N_ARGS_HELPER2( x1,         \
                        x2,         \
                        x3,         \
                        x4,         \
                        x5,         \
                        x6,         \
                        x7,         \
                        x8,         \
                        x9,         \
                        x10,        \
                        n,          \
                        ... ) n

#define CREATE_MACRO_EXPANSION( PREFIX, POSTFIX ) \
    PREFIX ## POSTFIX

#define BUILD_MACRO( PREFIX, POSTFIX ) \
    CREATE_MACRO_EXPANSION( PREFIX, POSTFIX )

// If a parameter is `NULL` returns `ERROR_ARGUMENT_IS_NULL`
#define __NOT_NULL( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NULL_RETURN_BWSR, __VA_ARGS__ );

// If a parameter is `NULL` returns `NULL`
#define __NOT_NULL_RETURN_NULL( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NULL_RETURN_NULL, __VA_ARGS__ );

// If a parameter is `NULL` returns
#define __NOT_NULL_RETURN_VOID( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NULL_RETURN_VOID, __VA_ARGS__ );

// If a parameter is `NULL` returns `0`
#define __NOT_NULL_RETURN_0( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NULL_RETURN_0, __VA_ARGS__ );

// If a parameter is not greater than `0` returns `ERROR_INVALID_ARGUMENT_VALUE`
#define __GREATER_THAN_0( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_BWSR, __VA_ARGS__ );

// If a parameter is not greater than `0` returns `NULL`
#define __GREATER_THAN_0_RETURN_NULL( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_NULL, __VA_ARGS__ );

// If a parameter is not greater than `0` returns
#define __GREATER_THAN_0_RETURN_VOID( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( IF_PARAMETER_NOT_GREATER_THAN_0_RETURN_VOID, __VA_ARGS__ );

// Avoids unused parameter warning by casting parameters to void
#define __UNUSED( ... ) \
    BUILD_MACRO( MACRO_EXPAND, N_ARGS( 0, __VA_ARGS__ ) )( UNUSED_PARAMETER, __VA_ARGS__ )

#endif