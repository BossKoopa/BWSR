#ifndef __ERROR_H__
#define __ERROR_H__

// -----------------------------------------------------------------------------
//  BASE ERRORS
// -----------------------------------------------------------------------------

#define ERROR_SUCCESS                       ( 0x00000000 )
#define ERROR_FAILURE                       ( 0xFFFFFFFF )

// -----------------------------------------------------------------------------
//  GENERIC ERRORS
// -----------------------------------------------------------------------------

#define ERROR_ARGUMENT_IS_NULL              ( 0x00000010 )
#define ERROR_INVALID_ARGUMENT_VALUE        ( 0x00000011 )
#define ERROR_NOT_FOUND                     ( 0x00000012 )
#define ERROR_UNHANDLED_DATA_TYPE           ( 0x00000013 )
#define ERROR_UNIMPLEMENTED                 ( 0x00000014 )
#define ERROR_UNEXPECTED_FORMAT             ( 0x00000015 )

// -----------------------------------------------------------------------------
//  MEMORY ERRORS
// -----------------------------------------------------------------------------

#define ERROR_MEM_ALLOC                     ( 0x00000100 )
#define ERROR_MEMORY_MAPPING                ( 0x00000101 )
#define ERROR_MEMORY_PERMISSION             ( 0x00000102 )
#define ERROR_MEMORY_OVERFLOW               ( 0x00000103 )

// -----------------------------------------------------------------------------
//  I/O ERRROS
// -----------------------------------------------------------------------------

#define ERROR_FILE_IO                       ( 0x00001000 )
#define ERROR_CACHED_LOCATION               ( 0x00001001 )
#define ERROR_SHARED_CACHE                  ( 0x00001002 )
#define ERROR_PROC_SELF_MAPS                ( 0x00001003 )

// -----------------------------------------------------------------------------
//  OS ERRORS
// -----------------------------------------------------------------------------

#define ERROR_SYMBOL_SIZE                   ( 0x00010000 )
#define ERROR_TASK_INFO                     ( 0x00010001 )
#define ERROR_ROUTING_FAILURE               ( 0x00010002 )

// -----------------------------------------------------------------------------
//  ERROR STRING CONVERSION
// -----------------------------------------------------------------------------

#define ERROR_STRINGS( E )                                                          \
    E( ERROR_SUCCESS,                   "Success"                               )   \
    E( ERROR_FAILURE,                   "Generic Error"                         )   \
    /* --- GENERICS --- */                                                          \
    E( ERROR_ARGUMENT_IS_NULL,          "An argument is NULL"                   )   \
    E( ERROR_INVALID_ARGUMENT_VALUE,    "An argument has a bad value"           )   \
    E( ERROR_NOT_FOUND,                 "Element not found"                     )   \
    E( ERROR_UNHANDLED_DATA_TYPE,       "Unexpected data type"                  )   \
    E( ERROR_UNIMPLEMENTED,             "No implementation for this data type"  )   \
    E( ERROR_UNEXPECTED_FORMAT,         "Data format did match expectation"     )   \
    /* --- MEMORY --- */                                                            \
    E( ERROR_MEM_ALLOC,                 "Out of memory"                         )   \
    E( ERROR_MEMORY_MAPPING,            "Faied to map memory region"            )   \
    E( ERROR_MEMORY_PERMISSION,         "Failed to change memory permissions"   )   \
    E( ERROR_MEMORY_OVERFLOW,           "Allocated memory not large enough"     )   \
    /* --- I/O --- */                                                               \
    E( ERROR_FILE_IO,                   "File I/O"                              )   \
    E( ERROR_CACHED_LOCATION,           "Invalid cache location"                )   \
    E( ERROR_SHARED_CACHE,              "Failed to initialize shared cache"     )   \
    E( ERROR_PROC_SELF_MAPS,            "Failed to open proc/self/maps"         )   \
    /* --- OS --- */                                                                \
    E( ERROR_SYMBOL_SIZE,               "Invalid symbol size"                   )   \
    E( ERROR_TASK_INFO,                 "Need to summarize"                     )   \
    E( ERROR_ROUTING_FAILURE,           "Failed to setup VirtualPage routing"   )

#define ERROR_TEXT( ERROR_CODE, TEXT ) \
    case ERROR_CODE: return #ERROR_CODE " (" TEXT ")";

typedef int BWSR_STATUS;

static inline const char* ErrorString( int ErrorCode )
{
    switch( ErrorCode )
    {
        ERROR_STRINGS( ERROR_TEXT )
    } // switch()

    return "Unknown Error code!";
}

#endif