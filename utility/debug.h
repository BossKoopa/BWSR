
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <syslog.h>
#include <inttypes.h>
#include "utility/error.h"

// system is unusable
#define LOG_EMERGENCY     LOG_EMERG
// critical logging level
#define LOG_CRITICAL      LOG_CRIT
// Error logging level
#define LOG_ERROR         LOG_ERR

#define LOG_DEFAULTS            ( LOG_CONS | LOG_PERROR | LOG_ODELAY | LOG_NDELAY | LOG_USER | LOG_PID )
// #define LOG_DEFAULTS            ( LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER )
#define LOG_NAME                "No_Name"
#define RET_CODE_8X             "0x%08" PRIX8

#if defined( DEBUG_MODE )

    #define BWSR_DEBUG( LOG_LEVEL,                  \
                        FORMAT,                     \
                        ARGUMENTS... )              \
        do                                          \
        {                                           \
            openlog( LOG_NAME,                      \
                    LOG_DEFAULTS,                   \
                    LOG_USER );                     \
            syslog( LOG_LEVEL,                      \
                    "%s[%d] -> %s(): " FORMAT,      \
                    __FILE__,                       \
                    __LINE__,                       \
                    __FUNCTION__,                   \
                    ##ARGUMENTS );                  \
            closelog();                             \
        }                                           \
        while( 0 );

    #define DEBUG( ... )                            \
        BWSR_DEBUG( LOG_DEBUG, __VA_ARGS__ );

    #define __DEBUG_ENTER                           \
        BWSR_DEBUG( LOG_DEBUG, "enter\n" );

    #define __DEBUG_EXIT                            \
        BWSR_DEBUG( LOG_DEBUG, "exit\n" );

    #define __DEBUG_RETVAL( RETURN_CODE )               \
        BWSR_DEBUG( LOG_DEBUG,                          \
                    "retVal: " RET_CODE_8X " %s\n",     \
                    RETURN_CODE,                        \
                    ErrorString( RETURN_CODE ) );

#else

    #define BWSR_DEBUG( LOG_LEVEL, FORMAT, ARGUMENTS... )
    #define DEBUG( ... )
    #define __DEBUG_ENTER
    #define __DEBUG_EXIT
    #define __DEBUG_RETVAL( RETURN_CODE )

#endif // DEBUG_MODE

#endif