
//------------------------------------------------------------------------------
//  INCLUDES
//------------------------------------------------------------------------------

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#ifdef __APPLE__
    #include <mach/mach.h>
    #include <AudioToolbox/AudioUnit.h>
    #include "SymbolResolve/Darwin/Macho.h"
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include "SymbolResolve/Linux/Elf.h"
#endif

#include "utility/debug.h"
#include "utility/error.h"
#include "utility/utility.h"

#include "Hook/InlineHook.h"

#include "Memory/MemoryTracker.h"

#ifndef DEBUG_MODE
    #ifdef BWSR_DEBUG
        #undef BWSR_DEBUG
    #endif

    #define BWSR_DEBUG( LEVEL, ARGS... ) fprintf( stderr, ##ARGS );
#endif

//------------------------------------------------------------------------------
//  HOOK FUNCTIONS AND HOOK TYPES
//------------------------------------------------------------------------------

#ifdef __APPLE__
typedef int32_t
    ( *AudioUnitProcess_t )
    (
        AudioUnit                       inUnit,
        AudioUnitRenderActionFlags*     __nullable ioActionFlags,
        const AudioTimeStamp*           inTimeStamp,
        UInt32                          inNumberFrames,
        AudioBufferList*                ioData
    );
#endif


int hcreat( const char * files, mode_t modes )
{
    BWSR_DEBUG( LOG_CRITICAL,
                "SUCCESS! Caught creat()!: '%s'\n",
                files );
    __UNUSED( files, modes )
    return -1;
}

int hprintf( const char* text, ... )
{
    BWSR_DEBUG( LOG_CRITICAL,
                "SUCCESS! Caught printf() with text: '%s'\n",
                text );
    __UNUSED( text )
    return -1;
}

#ifdef __APPLE__
int32_t hAudioUnitProcess
    (
        AudioUnit                       inUnit,
        AudioUnitRenderActionFlags*     __nullable ioActionFlags,
        const AudioTimeStamp*           inTimeStamp,
        UInt32                          inNumberFrames,
        AudioBufferList*                ioData
    )
{
    BWSR_DEBUG( LOG_CRITICAL, "SUCCESS! Caught AudioUnitProcess()!\n" );
    __UNUSED( inUnit,
              ioActionFlags,
              inTimeStamp,
              inNumberFrames,
              ioData )
    return -1;
}
#endif

//------------------------------------------------------------------------------
//  CODESIGN CALLBACKS
//------------------------------------------------------------------------------

void
    BeforePageWriteCallbackFn
    (
        uintptr_t       PageAddress
    )
{
    BWSR_DEBUG( LOG_CRITICAL, "PageAddress: %" PRIuPTR "\n", PageAddress );
    __UNUSED( PageAddress )
    // Uneeded for most people
}

void
    AfterPageWriteCallbackFn
    (
        uintptr_t       PageAddress
    )
{
    BWSR_DEBUG( LOG_ALERT, "PageAddress: %" PRIuPTR "\n", PageAddress );
    __UNUSED( PageAddress )
    // Rehash Code Page
    // Rehash CDHash
    // CodeDirectory stuff :)
}

//------------------------------------------------------------------------------
//  EXAMPLES
//------------------------------------------------------------------------------

static
void
    EXAMPLE_hooking_creat
    (
        void
    )
{
    BWSR_InlineHook( creat, hcreat, NULL, BeforePageWriteCallbackFn, AfterPageWriteCallbackFn );
    BWSR_DEBUG( LOG_DEBUG, "Calling creat()\n" );

    int fd = creat( "/var/mobile/creat_test_file1.txt", S_IRWXG | S_IRWXU | S_IRWXO );

    if( 0 < fd )
    {
        BWSR_DEBUG( LOG_DEBUG, "FAILURE! creat() call went through!!!\n" );
        close( fd );
    }

    BWSR_DEBUG( LOG_DEBUG, "Unhooking creat()\n" );
    BWSR_DestroyHook( creat );
}

static
void
    EXAMPLE_hooking_printf
    (
        void
    )
{
    void* oldprintf = NULL;

    BWSR_InlineHook( printf, hprintf, &oldprintf, NULL, NULL );

    printf( "Testing printf of an integer: %d", 1 );

    if( NULL != oldprintf )
    {
        BWSR_DEBUG( LOG_CRITICAL, "Calling original printf(). Console should display!\n" );

        if( EOF == ((__typeof(printf)*)oldprintf)( "Testing the original version of printf with integer: %d!\n", 1 ) )
        {
            BWSR_DEBUG( LOG_CRITICAL, "FAILURE: Original printf() did not write any bytes to console!\n" );
        }
        else {
            BWSR_DEBUG( LOG_CRITICAL, "SUCCESS: Original printf() worked!\n" );
        }
    }
    else {
        BWSR_DEBUG( LOG_CRITICAL, "FAILURE: original printf() could not be called!\n" );
    }
}

#if defined( __APPLE__ )
void
    EXAMPLE_hooking_AudioUnitProcess
    (
        void
    )
{
    uintptr_t                       aup_address = 0;
    AudioUnit                       inUnit = NULL;
    AudioUnitRenderActionFlags*     ioActionFlags = NULL;
    const AudioTimeStamp*           inTimeStamp = NULL;
    UInt32                          inNumberFrames = 0;
    AudioBufferList*                ioData = NULL;

    BWSR_ResolveSymbol( "AudioUnitProcess", NULL, &aup_address );
    BWSR_InlineHook( (void*)aup_address, hAudioUnitProcess, NULL, BeforePageWriteCallbackFn, AfterPageWriteCallbackFn );

    AudioUnitProcess( inUnit, ioActionFlags, inTimeStamp, inNumberFrames, ioData );
}
#else
void
    EXAMPLE_linux_SymbolResolve
    (
        void
    )
{
    uintptr_t open_address = 0;
    BWSR_ResolveSymbol( "open", NULL, &open_address );

    BWSR_DEBUG( LOG_CRITICAL, "open address: %p\n", open );
    BWSR_DEBUG( LOG_CRITICAL, "resolved address: 0x%lx\n", open_address );
}
#endif


int main()
{

#ifndef DEBUG_MODE
    fprintf( stderr, "Example was made without DEBUG printing. Output will be limited!\n" );
#endif

    EXAMPLE_hooking_creat();

    EXAMPLE_hooking_printf();

#if defined( __APPLE__ )

    EXAMPLE_hooking_AudioUnitProcess();

#else

    EXAMPLE_linux_SymbolResolve();

#endif

    BWSR_DEBUG( LOG_CRITICAL, "Cleaning up all hooks\n" );

    // Clean up all hooks
    BWSR_DestroyAllHooks();

#if defined( DEBUG_MODE )

    size_t leaks = MemoryTracker_CheckForMemoryLeaks();
    BWSR_DEBUG( LOG_CRITICAL,
                "%zu memory leaks found!\n",
                leaks );

#endif
}
