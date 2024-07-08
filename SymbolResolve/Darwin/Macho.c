
//------------------------------------------------------------------------------
//  INCLUDES
//------------------------------------------------------------------------------

#include <stdint.h>

#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld_images.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioUnit.h>

#include "utility/utility.h"
#include "utility/error.h"
#include "utility/debug.h"
#include "apple/dyld_cache_format.h"

//------------------------------------------------------------------------------
//  DYNAMICALLY LINKED EXTERNAL METHODS
//------------------------------------------------------------------------------

#ifdef __cplusplus
    extern "C" {
#endif

extern
const char*
    dyld_shared_cache_file_path
    (

    );

extern
int
    __shared_region_check_np
    (
        uint64_t*               StartAddress
    );

#ifdef __cplusplus
    }
#endif


//------------------------------------------------------------------------------
//  DATA STRUCTURES
//------------------------------------------------------------------------------

#if defined( __LP64__ )

    typedef struct mach_header_64                       mach_header_t;
    typedef struct segment_command_64                   segment_command_t;
    typedef struct section_64                           section_t;
    typedef struct nlist_64                             nlist_t;
    typedef struct dyld_cache_local_symbols_entry_64    dcls_entry_t;

    #define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64

#else

    typedef struct mach_header                          mach_header_t;
    typedef struct segment_command                      segment_command_t;
    typedef struct section                              section_t;
    typedef struct nlist                                nlist_t;
    typedef struct dyld_cache_local_symbols_entry       dcls_entry_t;

    #define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT

#endif

#define SYMBOLS_FILE_EXTENSION ".symbols"
#define MAX_SEGMENT_COUNT 64
#define OPTIONAL

typedef struct dyld_cache_header                    dyld_cache_header_t;
typedef struct dyld_cache_mapping_info              dyld_cache_mapping_info_t;
typedef struct dyld_all_image_infos                 dyld_all_image_infos_t;
typedef struct dyld_cache_local_symbols_info        dcls_info_t;
typedef struct load_command                         load_command_t;
typedef struct symtab_command                       symtab_command_t;
typedef struct dysymtab_command                     dysymtab_command_t;
typedef struct dyld_info_command                    dyld_info_command_t;
typedef struct linkedit_data_command                linkedit_data_command_t;

typedef struct
shared_cache_ctx
{
    // Dyld Caches
    dyld_cache_header_t*        RuntimeSharedCache;
    dyld_cache_header_t*        MmapSharedCache;

    // ASLR
    uintptr_t                   RuntimeSlide;

    // Symbols
    dcls_info_t*                LocalSymbolsInfo;
    dcls_entry_t*               LocalSymbolsEntries;

    // Tables
    nlist_t*                    SymbolTable;
    char*                       StringTable;
} shared_cache_ctx_t;

typedef struct
macho_ctx_t
{
    bool                        IsRuntimeMode;

    // Headers
    mach_header_t*              Header;
    mach_header_t*              CacheHeader;

    // VM Region
    uintptr_t                   LoadVmAddr;
    size_t                      VmSize;
    uintptr_t                   VmRegionStart;
    uintptr_t                   VmRegionEnd;

    // ASLR
    uintptr_t                   Slide;
    uintptr_t                   LinkedItBase;

    // Segments
    segment_command_t*          Segments[ MAX_SEGMENT_COUNT ];
    int                         SegmentsCount;
    segment_command_t*          TextSegment;
    segment_command_t*          DataSegment;
    segment_command_t*          TextExecSegment;
    segment_command_t*          DataConstSegment;
    segment_command_t*          LinkedItSegment;

    // Commands
    symtab_command_t*           SymbolTableCommand;                 // LC_SYMTAB
    dysymtab_command_t*         DySymbolTableCommand;               // LC_DYSYMTAB
    dyld_info_command_t*        DyldInfoCommand;                    // LC_DYLD_INFO or LC_DYLD_INFO_ONLY
    linkedit_data_command_t*    ExportsTrieCommand;                 // LC_DYLD_EXPORTS_TRIE
    linkedit_data_command_t*    ChainedFixupsCommand;               // LC_DYLD_CHAINED_FIXUPS
    linkedit_data_command_t*    CodeSignatureCommand;               // LC_CODE_SIGNATURE

    // Tables
    nlist_t*                    SymbolTable;
    char*                       StringTable;
    uint32_t*                   IndirectSymbolTable;
} macho_ctx_t;

//------------------------------------------------------------------------------
//  PROTOTYPES
//------------------------------------------------------------------------------

static
BWSR_STATUS
    INTERNAL_GetSharedCacheBaseAddress
    (
        OUT         dyld_cache_header_t**       LoadAddress
    );

static
BWSR_STATUS
    INTERNAL_SharedCacheContext_Initialize
    (
        IN  OUT     shared_cache_ctx_t*         Context
    );

static
BWSR_STATUS
    INTERNAL_MapFileOffsetToBuffer
    (
        IN          size_t                      MapSize,
        IN          off_t                       MapOffset,
        IN          const char*                 MapFile,
        OUT         uint8_t**                   MmapBuffer
    );

static
BWSR_STATUS
    INTERNAL_MapSharedCacheToBuffer
    (
        IN          const char*                 MapFile,
        IN  OUT     uint8_t**                   MmapBuffer
    );

static
BWSR_STATUS
    INTERNAL_LoadSymbolsFromSharedCache
    (
        IN          shared_cache_ctx_t*         Context
    );

static
BWSR_STATUS
    INTERNAL_IsAddressInSharedCache
    (
        IN          shared_cache_ctx_t*         Context,
        IN          uintptr_t                   Address
    );

static
BWSR_STATUS
    INTERNAL_LoadSymbolTableFromSharedCache
    (
        IN          shared_cache_ctx_t*         Context,
        IN          mach_header_t*              ImageHeader,
        OUT         nlist_t**                   SymbolTable,
        OUT         uint32_t*                   SymbolTableCount,
        OUT         char**                      StringTable
    );

static
BWSR_STATUS
    INTERNAL_FindSymbolAddressInSymbolTable
    (
        IN          char*               SymbolNamePattern,
        IN          nlist_t*            SymbolTable,
        IN          uint32_t            SymbolTableCount,
        IN          char*               StringTable,
        OUT         uintptr_t*          SymbolAddress
    );

static
BWSR_STATUS
    INTERNAL_UpdateContextFromLoadCommand
    (
        IN          load_command_t*         LoadCommand,
        OUT         macho_ctx_t*            Context
    );

static
BWSR_STATUS
    INTERNAL_MachoContext_Initialize
    (
        IN              mach_header_t*      MachHeader,
        IN              bool                RuntimeMode,
        IN OPTIONAL     mach_header_t*      CacheHeader,
        OUT             macho_ctx_t*        Context
    );

static
BWSR_STATUS
    INTERNAL_ResolveAddressWithSymbolTable
    (
        IN          macho_ctx_t*            Context,
        IN          const char*             SymbolNamePattern,
        IN OUT      uintptr_t*              SymbolAddress
    );

static
BWSR_STATUS
    INTERNAL_ResolveAddressForSymbol
    (
        IN  OUT     macho_ctx_t*            Context,
        IN          const char*             SymbolName,
        OUT         uintptr_t*              SymbolAddress
    );

static
BWSR_STATUS
    INTERNAL_ResolveSymbol_SharedCache
    (
        IN          const char*             SymbolName,
        IN          const mach_header_t*    MachHeader,
        IN  OUT     uintptr_t*              FunctionAddress
    );

static
BWSR_STATUS
    INTERNAL_ResolveSymbol_SymbolTable
    (
        IN          const char*             SymbolName,
        IN          const mach_header_t*    MachHeader,
        IN  OUT     uintptr_t*              FunctionAddress
    );

static
BWSR_STATUS
    INTERNAL_ResolveSymbol
    (
        IN              const char*             SymbolName,
        IN OPTIONAL     const char*             ImageName,
        OUT             uintptr_t*              Address
    );

//------------------------------------------------------------------------------
//  PRIVATE FUNCTIONS
//------------------------------------------------------------------------------

static
BWSR_STATUS
    INTERNAL_GetSharedCacheBaseAddress
    (
        OUT         dyld_cache_header_t**       BaseAddress
    )
{
    BWSR_STATUS                 retVal              = ERROR_FAILURE;
    uintptr_t                   sharedCacheBase     = 0;
    task_dyld_info_data_t       taskDyldInfo        = { 0 };
    mach_msg_type_number_t      count               = TASK_DYLD_INFO_COUNT;
    kern_return_t               taskInfoStatus      = KERN_FAILURE;
    dyld_all_image_infos_t*     allImageInfos       = NULL;

    __NOT_NULL( BaseAddress )

    if( 0 != __shared_region_check_np( (uint64_t*) &sharedCacheBase ) )
    {
        BWSR_DEBUG( LOG_WARNING, "__shared_region_check_np() Failed. Attempting task resolve.\n" );
    } // __shared_region_check_np()

    if( sharedCacheBase )
    {
        *BaseAddress    = (dyld_cache_header_t*) sharedCacheBase;
        retVal          = ERROR_SUCCESS;
    }
    else {
        if( KERN_SUCCESS != ( taskInfoStatus = task_info( mach_task_self(),
                                                          TASK_DYLD_INFO,
                                                          (task_info_t) &taskDyldInfo,
                                                          &count ) ) )
        {
            BWSR_DEBUG( LOG_ERROR,
                        "task_info() Failed. retVal: %d\n",
                        taskInfoStatus );
            retVal = ERROR_TASK_INFO;
        }
        else {
            allImageInfos   = (dyld_all_image_infos_t*) taskDyldInfo.all_image_info_addr;
            *BaseAddress    = (dyld_cache_header_t*) allImageInfos->sharedCacheBaseAddress;
            retVal          = ERROR_SUCCESS;
        } // task_info()
    } // shared_cache_base

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_SharedCacheContext_Initialize
    (
        IN  OUT     shared_cache_ctx_t*         Context
    )
{
    BWSR_STATUS                     retVal              = ERROR_FAILURE;
    dyld_cache_header_t*            rtSharedCache       = NULL;
    dyld_cache_mapping_info_t*      mappings            = NULL;
    uintptr_t                       slide               = 0;

    __NOT_NULL( Context )

    memset( Context,
            0,
            sizeof( shared_cache_ctx_t ) );

    if( ERROR_SUCCESS != ( retVal = INTERNAL_GetSharedCacheBaseAddress( &rtSharedCache ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_GetSharedCacheBaseAddress() Failed\n" );
    }
    else {
        Context->RuntimeSharedCache = rtSharedCache;

        mappings    = (dyld_cache_mapping_info_t*) ( (char*) rtSharedCache + rtSharedCache->mappingOffset );
        slide       = (uintptr_t) rtSharedCache - (uintptr_t) ( mappings[ 0 ].address );

        Context->RuntimeSlide = slide;
    } // INTERNAL_GetSharedCacheBaseAddress()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_MapFileOffsetToBuffer
    (
        IN          size_t                      MapSize,
        IN          off_t                       MapOffset,
        IN          const char*                 MapFile,
        OUT         uint8_t**                   MmapBuffer
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    int             fileDescriptor      = -1;

    __NOT_NULL( MapFile, MmapBuffer )

    if( 0 > ( fileDescriptor = open( MapFile,
                                     O_RDONLY,
                                     0 ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "open() Failed\n" );
        retVal = ERROR_FILE_IO;
    }
    else {
        if( MAP_FAILED == ( *MmapBuffer = (uint8_t *) mmap( 0,
                                                            MapSize,
                                                            PROT_READ | PROT_WRITE,
                                                            MAP_PRIVATE,
                                                            fileDescriptor,
                                                            MapOffset ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "mmap() Failed\n" );

            *MmapBuffer = NULL;
            retVal      = ERROR_MEMORY_MAPPING;
        }
        else {
            retVal = ERROR_SUCCESS;
        } // mmap()

        close( fileDescriptor );
    } // open()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_MapSharedCacheToBuffer
    (
        IN          const char*                 MapFile,
        IN  OUT     uint8_t**                   MmapBuffer
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    struct stat     fileStat        = { 0 };
    int             statCode        = 0;

    __NOT_NULL( MapFile, MmapBuffer )

    if( 0 != ( statCode = stat( MapFile, &fileStat ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "stat() Failed\n" );
        retVal = ERROR_FILE_IO;
    }
    else {
        retVal = INTERNAL_MapFileOffsetToBuffer( fileStat.st_size,
                                                 0,
                                                 MapFile,
                                                 MmapBuffer );
    } // stat()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_LoadSymbolsFromSharedCache
    (
        IN          shared_cache_ctx_t*         Context
    )
{
    BWSR_STATUS     retVal                                  = ERROR_SUCCESS;
    uint64_t        localSymbolsOffset                      = 0;
    char*           sharedCachePath                         = NULL;
    char            sharedCacheSymbolsPath[ PATH_MAX ]      = { 0 };
    uint8_t*        mmBuffer                                = NULL;

    __NOT_NULL( Context )

    if( NULL == ( sharedCachePath = (char*) dyld_shared_cache_file_path() ) )
    {
        BWSR_DEBUG( LOG_ERROR, "dyld_shared_cache_file_path() Failed\n" );
        retVal = ERROR_CACHED_LOCATION;
    }
    else {
        (void) strncat( sharedCacheSymbolsPath,
                        sharedCachePath,
                        PATH_MAX );
        (void) strncat( sharedCacheSymbolsPath,
                        SYMBOLS_FILE_EXTENSION,
                        sizeof( SYMBOLS_FILE_EXTENSION ) );

        if( ERROR_SUCCESS != ( retVal = INTERNAL_MapSharedCacheToBuffer( sharedCacheSymbolsPath, &mmBuffer ) ) )
        {
            // Probably iOS < 15.0
            if( 0 == Context->RuntimeSharedCache->localSymbolsSize )
            {
                BWSR_DEBUG( LOG_CRITICAL, "Context->RuntimeSharedCache->localSymbolsSize Invalid.\n" );
                retVal = ERROR_SYMBOL_SIZE;
            }
            else {
                if( ERROR_SUCCESS != ( retVal = INTERNAL_MapFileOffsetToBuffer( Context->RuntimeSharedCache->localSymbolsSize,
                                                                                Context->RuntimeSharedCache->localSymbolsOffset,
                                                                                sharedCachePath,
                                                                                &mmBuffer ) ) )
                {
                    BWSR_DEBUG( LOG_ERROR, "INTERNAL_MapFileOffsetToBuffer() Failed.\n" );
                }
                else {
                    Context->MmapSharedCache = (dyld_cache_header_t*)
                        ( (uintptr_t) mmBuffer - Context->RuntimeSharedCache->localSymbolsOffset );

                    localSymbolsOffset  = Context->RuntimeSharedCache->localSymbolsOffset;
                } // INTERNAL_MapFileOffsetToBuffer()
            } // Context->RuntimeSharedCache->localSymbolsSize
        }
        else {
            // iOS >= 15.0
            Context->MmapSharedCache    = (dyld_cache_header_t*)mmBuffer;
            localSymbolsOffset          = Context->MmapSharedCache->localSymbolsOffset;
        } // INTERNAL_MapSharedCacheToBuffer()

        if( ERROR_SUCCESS == retVal )
        {
            Context->LocalSymbolsInfo    = (dcls_info_t*)
                ( (char*)Context->MmapSharedCache + localSymbolsOffset );
            Context->LocalSymbolsEntries = (dcls_entry_t*)
                ( (char*)Context->LocalSymbolsInfo + Context->LocalSymbolsInfo->entriesOffset );
            Context->SymbolTable = (nlist_t*)
                ( (char*)Context->LocalSymbolsInfo + Context->LocalSymbolsInfo->nlistOffset );
            Context->StringTable = (char*)
                ( (char*)Context->LocalSymbolsInfo + Context->LocalSymbolsInfo->stringsOffset );
        } // ERROR_SUCCESS == retVal
    } // dyld_shared_cache_file_path()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_IsAddressInSharedCache
    (
        IN          shared_cache_ctx_t*         Context,
        IN          uintptr_t                   Address
    )
{
    BWSR_STATUS             retVal                  = ERROR_FAILURE;
    dyld_cache_header_t*    runtimeSharedCache      = NULL;
    uintptr_t               regionStart             = 0;
    uintptr_t               regionEnd               = 0;

    __NOT_NULL( Context )
    __GREATER_THAN_0( Address )

    if( Context )
    {
        runtimeSharedCache = Context->RuntimeSharedCache;
    }
    else
    {
        (void) INTERNAL_GetSharedCacheBaseAddress( &runtimeSharedCache );
    } // Context

    if( NULL == runtimeSharedCache )
    {
        BWSR_DEBUG( LOG_ERROR, "Failed to initialize shared cache!\n" );
        retVal = ERROR_SHARED_CACHE;
    }
    else {
        regionStart     = runtimeSharedCache->sharedRegionStart + Context->RuntimeSlide;
        regionEnd       = regionStart + runtimeSharedCache->sharedRegionSize;
        retVal          = ( ( ( Address >= regionStart ) && ( Address < regionEnd ) )
                            ? ERROR_SUCCESS
                            : ERROR_NOT_FOUND );
    } // runtimeSharedCache

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_LoadSymbolTableFromSharedCache
    (
        IN          shared_cache_ctx_t*         Context,
        IN          mach_header_t*              ImageHeader,
        OUT         nlist_t**                   SymbolTable,
        OUT         uint32_t*                   SymbolTableCount,
        OUT         char**                      StringTable
    )
{
    BWSR_STATUS     retVal              = ERROR_FAILURE;
    uint64_t        offsetInCache       = 0;
    nlist_t*        localNlists         = NULL;
    uint32_t        localNlistCount     = 0;
    uint32_t        itr                 = 0;
    uint32_t        localNlistStart     = 0;

    __NOT_NULL( Context,
                ImageHeader,
                SymbolTable,
                SymbolTableCount,
                StringTable )

    retVal        = ERROR_NOT_FOUND;
    offsetInCache = (uint64_t)ImageHeader - (uint64_t)Context->RuntimeSharedCache;

    for( itr = 0;
        itr < Context->LocalSymbolsInfo->entriesCount &&
        ERROR_NOT_FOUND == retVal;
        ++itr )
    {
        if( Context->LocalSymbolsEntries[ itr ].dylibOffset == offsetInCache )
        {
            localNlistStart = Context->LocalSymbolsEntries[ itr ].nlistStartIndex;
            localNlistCount = Context->LocalSymbolsEntries[ itr ].nlistCount;
            localNlists     = &Context->SymbolTable[ localNlistStart ];
            retVal          = ERROR_SUCCESS;
        }
    } // for()

    *SymbolTable        = localNlists;
    *SymbolTableCount   = (uint32_t) localNlistCount;
    *StringTable        = (char *) Context->StringTable;

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_FindSymbolAddressInSymbolTable
    (
        IN          char*               SymbolNamePattern,
        IN          nlist_t*            SymbolTable,
        IN          uint32_t            SymbolTableCount,
        IN          char*               StringTable,
        OUT         uintptr_t*          SymbolAddress
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    char*           symbolName      = NULL;
    uint32_t        i               = 0;

    __NOT_NULL( SymbolNamePattern,
                SymbolTable,
                StringTable,
                SymbolAddress )
    __GREATER_THAN_0( SymbolTableCount )

    *SymbolAddress  = 0;
    retVal          = ERROR_NOT_FOUND;

    for( i = 0;
        ( i <  SymbolTableCount ) &&
        ( 0 == *SymbolAddress   );
        i++ )
    {
        if( SymbolTable[ i ].n_value )
        {
            symbolName = StringTable + SymbolTable[ i ].n_un.n_strx;

            if( 0 == strcmp( SymbolNamePattern, symbolName ) )
            {
                *SymbolAddress = SymbolTable[ i ].n_value;
                retVal = ERROR_SUCCESS;
            }
            else if( ( '_' == symbolName[ 0 ] ) &&
                     ( 0   == strcmp( SymbolNamePattern, &symbolName[ 1 ] ) ) )
            {
                *SymbolAddress = SymbolTable[ i ].n_value;
                retVal = ERROR_SUCCESS;
            } // strcmp()
        } // SymbolTable[ sym_tab_itr ].n_value
    } // for()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_UpdateContextFromLoadCommand
    (
        IN          load_command_t*         LoadCommand,
        OUT         macho_ctx_t*            Context
    )
{
    BWSR_STATUS             retVal              = ERROR_SUCCESS;
    segment_command_t*      segmentCommand      = NULL;

    // BWSR_DEBUG( LOG_NOTICE,
    //             "Read LC: %s\n",
    //             LoadCommandString( LoadCommand->cmd ) );

    switch( LoadCommand->cmd )
    {
        case LC_SEGMENT_ARCH_DEPENDENT:
        {
            segmentCommand = (segment_command_t*) LoadCommand;
            Context->Segments[ Context->SegmentsCount++ ] = segmentCommand;

            if( !strcmp( segmentCommand->segname, "__LINKEDIT" ) )
            {
                Context->LinkedItSegment = segmentCommand;
            }
            else if( !strcmp( segmentCommand->segname, "__DATA" ) )
            {
                Context->DataSegment = segmentCommand;
            }
            else if( !strcmp( segmentCommand->segname, "__DATA_CONST" ) )
            {
                Context->DataConstSegment = segmentCommand;
            }
            else if( !strcmp( segmentCommand->segname, "__TEXT" ) )
            {
                Context->TextSegment = segmentCommand;
            }
            else if( !strcmp( segmentCommand->segname, "__TEXT_EXEC" ) )
            {
                Context->TextExecSegment = segmentCommand;
            }
            else {
                // BWSR_DEBUG( LOG_WARNING,
                //             "Unhandled segment: %s\n",
                //             segmentCommand->segname );
            } // strcmp()
            break;
        }

        case LC_SYMTAB:
        {
            Context->SymbolTableCommand = (symtab_command_t*) LoadCommand;
            break;
        }

        case LC_DYSYMTAB:
        {
            Context->DySymbolTableCommand = (dysymtab_command_t*) LoadCommand;
            break;
        }

        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY:
        {
            Context->DyldInfoCommand = (dyld_info_command_t*) LoadCommand;
            break;
        }

        case LC_DYLD_EXPORTS_TRIE:
        {
            Context->ExportsTrieCommand = (linkedit_data_command_t*) LoadCommand;
            break;
        }

        case LC_DYLD_CHAINED_FIXUPS:
        {
            Context->ChainedFixupsCommand = (linkedit_data_command_t*) LoadCommand;
            break;
        }

        case LC_CODE_SIGNATURE:
        {
            Context->CodeSignatureCommand = (linkedit_data_command_t*) LoadCommand;
            break;
        }

        default:
        {
            retVal = ERROR_UNHANDLED_DATA_TYPE;
            // BWSR_DEBUG( LOG_WARNING,
            //         "Unhandle Load Command: 0x%" PRIx8 "\n",
            //         LoadCommand->cmd );
            break;
        }
    } // switch()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_MachoContext_Initialize
    (
        IN              mach_header_t*      MachHeader,
        IN              bool                RuntimeMode,
        IN OPTIONAL     mach_header_t*      CacheHeader,
        OUT             macho_ctx_t*        Context
    )
{
    BWSR_STATUS         retVal          = ERROR_SUCCESS;
    load_command_t*     curr_cmd        = NULL;
    segment_command_t*  seg_cmd         = NULL;
    uint32_t            i               = 0;

    __NOT_NULL( MachHeader, Context )

    Context->Header         = MachHeader;
    Context->IsRuntimeMode  = RuntimeMode;
    Context->CacheHeader    = CacheHeader;

    curr_cmd = (load_command_t*)( (uintptr_t)MachHeader + sizeof( mach_header_t ) );

    for( i = 0; i < MachHeader->ncmds; i++ )
    {
        (void)INTERNAL_UpdateContextFromLoadCommand( curr_cmd, Context );
        curr_cmd = (load_command_t*)( (uintptr_t)curr_cmd + curr_cmd->cmdsize );
    } // for()

    Context->Slide          = (uintptr_t)MachHeader
                            - (uintptr_t)Context->TextSegment->vmaddr;
    Context->LinkedItBase   = (uintptr_t)Context->Slide
                            + Context->LinkedItSegment->vmaddr
                            - Context->LinkedItSegment->fileoff;

    if( !RuntimeMode )
    {
        Context->LinkedItBase = (uintptr_t)( CacheHeader ? CacheHeader : MachHeader );
    }

    Context->VmRegionStart  = (uintptr_t)-1;
    Context->VmRegionEnd    = 0;

    for( i = 0; i < (uint32_t)Context->SegmentsCount; i++ )
    {
        seg_cmd = Context->Segments[ i ];

        if( !strcmp( seg_cmd->segname, "__PAGEZERO" ) )
        {
            continue;
        }
        if( !strcmp( seg_cmd->segname, "__TEXT" ) )
        {
            Context->LoadVmAddr = seg_cmd->vmaddr;
        }

        if( Context->VmRegionStart > seg_cmd->vmaddr )
        {
            Context->VmRegionStart = seg_cmd->vmaddr;
        }
        if( Context->VmRegionEnd < ( seg_cmd->vmaddr + seg_cmd->vmsize ) )
        {
            Context->VmRegionEnd    = seg_cmd->vmaddr + seg_cmd->vmsize;
        }
    } // for()

    Context->VmSize = Context->VmRegionEnd - Context->VmRegionStart;

    if( Context->SymbolTableCommand )
    {
        Context->SymbolTable = (nlist_t*)( Context->LinkedItBase + Context->SymbolTableCommand->symoff );
        Context->StringTable = (char*)( Context->LinkedItBase + Context->SymbolTableCommand->stroff );
    }

    if( Context->DySymbolTableCommand )
    {
        Context->IndirectSymbolTable = (uint32_t*)( Context->LinkedItBase + Context->DySymbolTableCommand->indirectsymoff );
    }

    // BWSR_DEBUG( LOG_INFO,
    //             "Header: %p: region: %" PRIuPTR
    //             " - %" PRIuPTR
    //             ", load_vmaddr: %" PRIuPTR
    //             ", vmsize: %zu, slide: %" PRIuPTR "\n",
    //             Context->Header,
    //             Context->VmRegionStart,
    //             Context->VmRegionEnd,
    //             Context->LoadVmAddr,
    //             Context->VmSize,
    //             Context->Slide );

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ResolveAddressWithSymbolTable
    (
        IN          macho_ctx_t*            Context,
        IN          const char*             SymbolNamePattern,
        IN OUT      uintptr_t*              SymbolAddress
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    char*           symbolName      = NULL;
    uint32_t        i               = 0;

    __NOT_NULL( Context,
                SymbolNamePattern,
                SymbolAddress )

    *SymbolAddress  = 0;
    retVal          = ERROR_NOT_FOUND;

    for( i = 0;
        ( i < Context->SymbolTableCommand->nsyms ) &&
        ( ERROR_NOT_FOUND == retVal );
        i++ )
    {
        if( Context->SymbolTable[ i ].n_value )
        {
            symbolName = Context->StringTable + Context->SymbolTable[ i ].n_un.n_strx;

            if( 0 == strcmp( SymbolNamePattern, symbolName ) )
            {
                *SymbolAddress = Context->SymbolTable[ i ].n_value;
                retVal = ERROR_SUCCESS;
            }
            else if( ( '_' == symbolName[ 0 ]                               ) &&
                     ( 0   == strcmp( SymbolNamePattern, &symbolName[ 1 ] ) ) )
            {
                *SymbolAddress = Context->SymbolTable[ i ].n_value;
                retVal = ERROR_SUCCESS;
            } // strcmp()
        } // Context->symtab[ symbol_itr ].n_value
    } // for()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ResolveAddressForSymbol
    (
        IN  OUT     macho_ctx_t*            Context,
        IN          const char*             SymbolName,
        OUT         uintptr_t*              SymbolAddress
    )
{
    BWSR_STATUS  retVal = ERROR_FAILURE;

    __NOT_NULL( Context,
                SymbolName,
                SymbolAddress )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_ResolveAddressWithSymbolTable( Context,
                                                                            SymbolName,
                                                                            SymbolAddress ) ) )
    {
        /**
         * NOTE: This can be noisy if no image name was given. Since each
         * image needs to be check individually. In this case failures
         * are normal until a proper match is found.
         */
        // BWSR_DEBUG( LOG_ERROR, "INTERNAL_ResolveAddressWithSymbolTable() Failed\n" );
    }
    else {
        *SymbolAddress = *SymbolAddress + ( Context->IsRuntimeMode ? Context->Slide : 0 );
    } // INTERNAL_ResolveAddressWithSymbolTable()

    return retVal;
}


static
BWSR_STATUS
    INTERNAL_ResolveSymbol_SharedCache
    (
        IN          const char*             SymbolName,
        IN          const mach_header_t*    MachHeader,
        IN  OUT     uintptr_t*              FunctionAddress
    )
{
    BWSR_STATUS         retVal              = ERROR_FAILURE;
    nlist_t*            symbolTable         = NULL;
    uint32_t            symbolTableCount    = 0;
    char*               stringTable         = NULL;
    shared_cache_ctx_t  context             = { 0 };

    __NOT_NULL( SymbolName,
                MachHeader,
                FunctionAddress )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_SharedCacheContext_Initialize( &context ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_SharedCacheContext_Initialize() Failed\n" );
    }
    else {
        if( ERROR_SUCCESS != ( retVal = INTERNAL_LoadSymbolsFromSharedCache( &context ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "INTERNAL_LoadSymbolsFromSharedCache() Failed\n" );
        }
        else {
            if( NULL != context.MmapSharedCache )
            {
                if( ERROR_SUCCESS != ( retVal = INTERNAL_IsAddressInSharedCache( &context, (uintptr_t)MachHeader ) ) )
                {
                    BWSR_DEBUG( LOG_ERROR, "INTERNAL_IsAddressInSharedCache() Failed\n" );
                }
                else {
                    retVal = INTERNAL_LoadSymbolTableFromSharedCache( &context,
                                                                      (mach_header_t*)MachHeader,
                                                                      &symbolTable,
                                                                      &symbolTableCount,
                                                                      &stringTable );
                } // INTERNAL_IsAddressInSharedCache()
            } // shared_cache_ctx.MmapSharedCache

            if( ( NULL != symbolTable ) &&
                ( NULL != stringTable ) )
            {
                if( ERROR_SUCCESS != ( retVal = INTERNAL_FindSymbolAddressInSymbolTable( (char*)SymbolName,
                                                                                         symbolTable,
                                                                                         symbolTableCount,
                                                                                         stringTable,
                                                                                         FunctionAddress ) ) )
                {
                    /**
                     * NOTE: This can be noisy if no image name was given. Since each
                     * image needs to be check individually. In this case failures
                     * are normal until a proper match is found.
                     */
                    // BWSR_DEBUG( LOG_ERROR, "INTERNAL_FindSymbolAddressInSymbolTable() Failed\n" );
                }
                else {
                    BWSR_DEBUG( LOG_INFO,
                                "Address: %" PRIuPTR ", Slide: %" PRIuPTR "\n",
                                *FunctionAddress,
                                context.RuntimeSlide );

                    *FunctionAddress = *FunctionAddress + context.RuntimeSlide;
                } // INTERNAL_FindSymbolAddressInSymbolTable()
            } // symbolTable && stringTable
        } // INTERNAL_LoadSymbolsFromSharedCache()
    } // INTERNAL_SharedCacheContext_Initialize()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ResolveSymbol_SymbolTable
    (
        IN          const char*             SymbolName,
        IN          const mach_header_t*    MachHeader,
        IN  OUT     uintptr_t*              FunctionAddress
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    macho_ctx_t     machoContext    = { 0 };

    __NOT_NULL( SymbolName,
                MachHeader,
                FunctionAddress )

    if( ERROR_SUCCESS != ( retVal = INTERNAL_MachoContext_Initialize( (mach_header_t*)MachHeader,
                                                                      true,
                                                                      NULL,
                                                                      &machoContext ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "INTERNAL_MachoContext_Initialize() Failed\n" );
    }
    else {
        retVal = INTERNAL_ResolveAddressForSymbol( &machoContext,
                                                   SymbolName,
                                                   FunctionAddress );
    } // INTERNAL_MachoContext_Initialize()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ResolveSymbol
    (
        IN              const char*             SymbolName,
        IN OPTIONAL     const char*             ImageName,
        OUT             uintptr_t*              Address
    )
{
    BWSR_STATUS         retVal          = ERROR_FAILURE;
    uint32_t            count           = 0;
    uint32_t            i               = 0;
    char*               imageName       = NULL;
    mach_header_t*      machHeader      = NULL;

    __NOT_NULL( SymbolName, Address )

    count           = _dyld_image_count();
    retVal          = ERROR_NOT_FOUND;

    for( i = 0; ( i < count ) && ( ERROR_NOT_FOUND == retVal ); i++ )
    {

        if( NULL == ( imageName = (char*) _dyld_get_image_name( i ) ) )
        {
            continue;
        } // _dyld_get_image_name

        if( ( NULL != ImageName           ) &&
            ( NULL == strnstr( imageName,
                               ImageName,
                               PATH_MAX ) ) )
        {
            continue;
        } // strnstr()

        if( NULL == ( machHeader = (mach_header_t*) _dyld_get_image_header( i ) ) )
        {
            continue;
        } // _dyld_get_image_header()

        if( ERROR_SUCCESS != ( retVal = INTERNAL_ResolveSymbol_SharedCache( SymbolName,
                                                                            machHeader,
                                                                            Address ) ) )
        {
            retVal = INTERNAL_ResolveSymbol_SymbolTable( SymbolName,
                                                         machHeader,
                                                         Address );
        } // INTERNAL_ResolveSymbol_SharedCache()
    } // for()

    return retVal;
}

BWSR_API
BWSR_STATUS
    BWSR_ResolveSymbol
    (
        IN              const char*             SymbolName,
        IN OPTIONAL     const char*             ImageName,
        OUT             uintptr_t*              Address
    )
{
    BWSR_STATUS retVal = ERROR_FAILURE;

    __NOT_NULL( SymbolName, Address )

    retVal = INTERNAL_ResolveSymbol( SymbolName,
                                     ImageName,
                                     Address );

    __DEBUG_RETVAL( retVal )
    return retVal;
}
