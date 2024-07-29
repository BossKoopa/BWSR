
// -----------------------------------------------------------------------------
//  INCLUDES
// -----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <elf.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <limits.h>

#include "utility/debug.h"
#include "utility/error.h"
#include "utility/utility.h"

#include "Memory/Memory.h"

// -----------------------------------------------------------------------------
//  STRUCTURES & DEFINITIONS
// -----------------------------------------------------------------------------

#ifndef LINE_MAX
    #define LINE_MAX                ( 1024 )
#endif

#define EXTENSION_LENGTH            ( 7 )
#define MODULE_BASE_CAPACITY        ( 16 )

#if defined(__LP64__)

    typedef Elf64_Shdr  elf_shdr_t;
    typedef Elf64_Sym   elf_sym_t;
    typedef Elf64_Addr  elf_addr_t;
    typedef Elf64_Dyn   elf_dyn_t;
    typedef Elf64_Phdr  elf_phdr_t;
    typedef Elf64_Ehdr  elf_ehdr_t;

#else

    typedef Elf32_Shdr  elf_shdr_t;
    typedef Elf32_Sym   elf_sym_t;
    typedef Elf32_Addr  elf_addr_t;
    typedef Elf32_Dyn   elf_dyn_t;
    typedef Elf32_Phdr  elf_phdr_t;
    typedef Elf32_Ehdr  elf_ehdr_t;

#endif

typedef struct elf_ctx {
    void*           Header;

    uintptr_t       LoadBias;

    elf_shdr_t*     SymbolSh;
    elf_shdr_t*     DynamicSymbolSh;

    const char*     StringTable;
    elf_sym_t*      SymbolTable;

    const char*     DynamicStringTable;
    elf_sym_t*      DynamicSymbolTable;
} elf_ctx_t;

typedef struct runtime_module_t {
    void*     Base;
    char      Path[ 1024 ];
} runtime_module_t;

// -----------------------------------------------------------------------------
//  GLOBALS
// -----------------------------------------------------------------------------

static struct {
    runtime_module_t*   Data;
    size_t              Size;
    size_t              Capacity;
} modules = {
    NULL,
    0,
    0
};

// -----------------------------------------------------------------------------
//  IMPLEMENTATION
// -----------------------------------------------------------------------------

static
BWSR_STATUS
    INTERNAL_AppendRuntimeModule
    (
        IN          runtime_module_t        Module
    )
{
    BWSR_STATUS         retVal          = ERROR_FAILURE;
    runtime_module_t*   runtimeModule   = NULL;
    size_t              allocationSize  = 0;

    if( NULL == modules.Data )
    {
        modules.Capacity    = MODULE_BASE_CAPACITY;
        allocationSize      = modules.Capacity * sizeof( runtime_module_t );

        if( NULL == ( modules.Data = BwsrMalloc( allocationSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "BwsrMalloc() Failed\n" );
            retVal = ERROR_MEM_ALLOC;
        }
        else {
            retVal = ERROR_SUCCESS;
        } // BwsrMalloc()
    }
    else if( modules.Size >= modules.Capacity )
    {
        modules.Capacity    *= 2;
        allocationSize      = modules.Capacity * sizeof( runtime_module_t );

        if( NULL == ( runtimeModule = BwsrRealloc( modules.Data, allocationSize ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "BwsrRealloc() Failed\n" );
            retVal = ERROR_MEM_ALLOC;
        }
        else {
            modules.Data = runtimeModule;
            retVal = ERROR_SUCCESS;
        } // BwsrRealloc()
    }
    else {
        retVal = ERROR_SUCCESS;
    } // Module Capacity

    if( ERROR_SUCCESS == retVal )
    {
        modules.Data[ modules.Size++ ] = Module;
    }

    return retVal;
}

static
void
    INTERNAL_ReleaseRuntimeModules
    (
        void
    )
{
    BwsrFree( modules.Data );
    modules.Data      = NULL;
    modules.Size      = 0;
    modules.Capacity  = 0;
}

static
BWSR_STATUS
    INTERNAL_GetProcessMap_ProcSelfMaps
    (
        void
    )
{
    BWSR_STATUS     retVal                          = ERROR_FAILURE;
    FILE*           fp                              = NULL;
    char            line_buffer[ LINE_MAX + 1 ]     = { 0 };
    uintptr_t       region_start                    = 0;
    uintptr_t       region_end                      = 0;
    uintptr_t       region_offset                   = 0;
    char            permissions[ 5 ]                = { 0 };
    uint8_t         dev_major                       = 0;
    uint8_t         dev_minor                       = 0;
    long            inode                           = 0;
    int             path_index                      = 0;
    char*           path_buffer                     = NULL;

    if( NULL == ( fp = fopen( "/proc/self/maps", "r" ) ) )
    {
        BWSR_DEBUG( LOG_ERROR, "fopen() Failed\n" );
        retVal = ERROR_PROC_SELF_MAPS;
    }
    else {
        retVal = ERROR_SUCCESS;

        while( !feof( fp ) && ERROR_SUCCESS == retVal )
        {
            memset( line_buffer, 0, sizeof( line_buffer ) );

            if( NULL == fgets( line_buffer,
                                sizeof( line_buffer ),
                                fp ) )
            {
                break;
            }

            if( ( LINE_MAX == strnlen( line_buffer, LINE_MAX ) ) &&
                ( '\n'     != line_buffer[ LINE_MAX ]          ) )
            {
                int c = 0;

                do
                {
                    c = getc( fp );
                } while( ( EOF != c ) && ( '\n' != c ) );

                if( EOF == c )
                {
                    break;
                }
            }

            if( 7 > sscanf( line_buffer,
                            "%lx-%lx %4c %lx %hhx:%hhx %ld %n",
                            &region_start,
                            &region_end,
                            permissions,
                            &region_offset,
                            &dev_major,
                            &dev_minor,
                            &inode,
                            &path_index ) )
            {
                BWSR_DEBUG( LOG_ERROR, "sscanf() Failed\n" );
                INTERNAL_ReleaseRuntimeModules( );
                retVal = ERROR_UNEXPECTED_FORMAT;
            }
            else {
                if( ( 0 != strcmp( permissions, "r--p" ) ) &&
                    ( 0 != strcmp( permissions, "r-xp" ) ) )
                {
                    continue;
                }

                if( 0 != memcmp( ( (Elf64_Ehdr*) region_start )->e_ident,
                                ELFMAG,
                                SELFMAG ) )
                {
                    continue;
                }

                path_buffer = line_buffer + path_index;

                if( ( 0    == *path_buffer ) ||
                    ( '\n' == *path_buffer ) ||
                    ( '['  == *path_buffer ) )
                {
                    continue;
                }

                if( '\n' == path_buffer[ strlen( path_buffer ) - 1 ] )
                {
                    path_buffer[ strlen( path_buffer ) - 1 ] = 0x00;
                }

                runtime_module_t module;

                strncpy( module.Path,
                        path_buffer,
                        sizeof( module.Path ) - 1 );

                module.Base = (void*) region_start;

                retVal = INTERNAL_AppendRuntimeModule( module );
            } // sscanf()
        } // while()

        fclose( fp );
    } // fopen()

    return retVal;
}

static
void
    INTERNAL_ElfContext_Initialize
    (
        OUT         elf_ctx_t*          Context,
        IN          const void*         Header
    )
{
    size_t          i               = 0;
    elf_phdr_t*     phdr            = NULL;
    elf_shdr_t*     shstr_sh        = NULL;
    char*           shstrtab        = NULL;
    elf_shdr_t*     shdr            = NULL;
    elf_ehdr_t*     ehdr            = NULL;
    elf_addr_t      ehdr_addr       = 0;

    __NOT_NULL_RETURN_VOID( Context, Header );

    ehdr            = (elf_ehdr_t*) Header;
    ehdr_addr       = (elf_addr_t) ehdr;
    Context->Header = ehdr;

    // Dynamic Segment
    {
        phdr = (elf_phdr_t*) ( ehdr_addr + ehdr->e_phoff );

        for( i = 0; i < ehdr->e_phnum; i++ )
        {
            if( ( PT_LOAD == phdr[ i ].p_type  ) &&
                ( 0       == Context->LoadBias ) )
            {
                Context->LoadBias = ehdr_addr - ( phdr[ i ].p_vaddr - phdr[ i ].p_offset );
            }
            else if( PT_PHDR == phdr[ i ].p_type )
            {
                Context->LoadBias = (elf_addr_t) phdr - phdr[ i ].p_vaddr;
            } // P Type
        } // for()
    } // Dynamic Segment

    // Section
    {
        shdr     = (elf_shdr_t*) ( ehdr_addr + ehdr->e_shoff );
        shstr_sh = &shdr[ ehdr->e_shstrndx ];
        shstrtab = (char*) ( (uintptr_t) ehdr_addr + shstr_sh->sh_offset );

        for( i = 0; i < ehdr->e_shnum; i++ )
        {
            if( SHT_SYMTAB == shdr[ i ].sh_type )
            {
                Context->SymbolSh = &shdr[ i ];
                Context->SymbolTable = (elf_sym_t*) ( ehdr_addr + shdr[ i ].sh_offset );
            }
            else if( ( SHT_STRTAB == shdr[ i ].sh_type ) &&
                     ( 0          == strncmp( ( shstrtab + shdr[ i ].sh_name ),
                                              ".strtab",
                                              EXTENSION_LENGTH ) ) )
            {
                Context->StringTable = (const char*) ( ehdr_addr + shdr[ i ].sh_offset );
            }
            else if( SHT_DYNSYM == shdr[ i ].sh_type )
            {
                Context->DynamicSymbolSh = &shdr[ i ];
                Context->DynamicSymbolTable = (elf_sym_t*) ( ehdr_addr + shdr[ i ].sh_offset );
            }
            else if( ( SHT_STRTAB == shdr[ i ].sh_type ) &&
                     ( 0          == strncmp( ( shstrtab + shdr[ i ].sh_name ),
                                              ".dynstr",
                                              EXTENSION_LENGTH ) ) )
            {
                Context->DynamicStringTable = (const char*) ( ehdr_addr + shdr[ i ].sh_offset );
            } // SH Type
        } // for()
    } // Section
}

static
BWSR_STATUS
    INTERNAL_MMapModulePath
    (
        OUT         uint8_t**           MMapBuffer,
        OUT         size_t*             MMapBufferSize,
        IN          const uint8_t*      ModulePath
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    size_t          file_size       = 0;
    struct stat     s               = { 0 };
    int             fd              = 0;

    __NOT_NULL( MMapBuffer, ModulePath )

    if( 0 != stat( (const char*) ModulePath, &s ) )
    {
        BWSR_DEBUG( LOG_ERROR, "stat() Failed\n" );
        retVal = ERROR_FILE_IO;
    }
    else {
        file_size = s.st_size;

        if( 0 > ( fd = open( (const char*) ModulePath,
                             O_RDONLY,
                             0 ) ) )
        {
            BWSR_DEBUG( LOG_ERROR, "open() Failed\n" );
            retVal = ERROR_FILE_IO;
        }
        else {
            if( MAP_FAILED == ( *MMapBuffer = (uint8_t*) mmap( 0,
                                                               file_size,
                                                               ( PROT_READ | PROT_WRITE ),
                                                               MAP_PRIVATE,
                                                               fd,
                                                               0 ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "mmap() Failed\n" );
                retVal = ERROR_MEMORY_MAPPING;
            }
            else {
                if( NULL != MMapBufferSize )
                {
                    *MMapBufferSize = file_size;
                } // mmap_buffer_size

                retVal = ERROR_SUCCESS;
            } // mmap()

            close(fd);
        } // open()
    } // stat()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_GetValueFromSymbolTable
    (
        IN          const char*         SymbolName,
        IN          elf_sym_t*          SymbolTable,
        IN          const char*         StringTable,
        IN          size_t              Count,
        OUT         void**              Value
    )
{
    BWSR_STATUS     retVal          = ERROR_FAILURE;
    size_t          i               = 0;
    elf_sym_t*      sym             = NULL;
    char*           symbol_name     = NULL;

    __NOT_NULL( SymbolName,
                SymbolTable,
                StringTable,
                Value )
    __GREATER_THAN_0( Count )

    retVal  = ERROR_NOT_FOUND;
    *Value  = NULL;

    for( i = 0; ( i < Count ) && ( ERROR_NOT_FOUND == retVal ); ++i )
    {
        sym         = SymbolTable + i;
        symbol_name = (char*) StringTable + sym->st_name;

        if( 0 == strcmp( (const char*) symbol_name, SymbolName ) )
        {
            retVal  = ERROR_SUCCESS;
            *Value  = (void*) sym->st_value;
        } // strcmp()
    } // for()

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ElfContext_GetValueFromSymbolTable
    (
        IN          elf_ctx_t*          Context,
        IN          const char*         SymbolName,
        OUT         void**              Result
    )
{
    BWSR_STATUS     retVal      = ERROR_FAILURE;
    size_t          count       = 0;

    __NOT_NULL( Context,
                SymbolName,
                Result )

    if( ( NULL != Context->SymbolTable ) &&
        ( NULL != Context->StringTable ) )
    {
        count   = Context->SymbolSh->sh_size / sizeof( elf_sym_t );

        retVal  = INTERNAL_GetValueFromSymbolTable( SymbolName,
                                                    Context->SymbolTable,
                                                    Context->StringTable,
                                                    count,
                                                    Result );
    }

    if( ERROR_SUCCESS != retVal )
    {
        if( ( NULL != Context->DynamicSymbolTable ) &&
            ( NULL != Context->DynamicStringTable ) )
        {
            count   = Context->DynamicSymbolSh->sh_size / sizeof( elf_sym_t );

            retVal  = INTERNAL_GetValueFromSymbolTable( SymbolName,
                                                        Context->DynamicSymbolTable,
                                                        Context->DynamicStringTable,
                                                        count,
                                                        Result );
        }
    }

    return retVal;
}

static
BWSR_STATUS
    INTERNAL_ResolveSymbol
    (
        IN          const char*         LibraryName,
        IN          const char*         SymbolName,
        OUT         uintptr_t*          Address
    )
{
    BWSR_STATUS         retVal      = ERROR_FAILURE;
    uint8_t*            file_mem    = NULL;
    size_t              i           = 0;
    runtime_module_t*   module      = NULL;
    elf_ctx_t           context     = { 0 };

    __NOT_NULL( SymbolName, Address )

    *Address = 0;

    for( i = 0; ( i < modules.Size ) && ( 0 == *Address ); i++ )
    {
        module = &modules.Data[ i ];

        if( ( NULL != LibraryName ) &&
            ( 0    != strncmp( LibraryName,
                               modules.Data[ i ].Path,
                               PATH_MAX ) ) )
        {
            continue;
        } // strncmp()

        if( module->Base )
        {
            if( ERROR_SUCCESS != ( retVal = INTERNAL_MMapModulePath( &file_mem,
                                                                     NULL,
                                                                     (const uint8_t*) module->Path ) ) )
            {
                BWSR_DEBUG( LOG_ERROR, "INTERNAL_MMapModulePath() Failed\n" );
            }
            else {
                memset( &context,
                        0,
                        sizeof( elf_ctx_t ) );

                INTERNAL_ElfContext_Initialize( &context, file_mem );

                if( ERROR_SUCCESS != ( retVal = INTERNAL_ElfContext_GetValueFromSymbolTable( &context,
                                                                                             SymbolName,
                                                                                             (void**) Address ) ) )
                {
                    BWSR_DEBUG( LOG_WARNING, "INTERNAL_ElfContext_GetValueFromSymbolTable() Failed. Retrying.\n" );
                }
                else {
                    if( *Address )
                    {
                        *Address = ( (uintptr_t) *Address
                                    + (uintptr_t) module->Base
                                    - ( (uintptr_t) file_mem - (uintptr_t) context.LoadBias ) );
                        retVal  = ERROR_SUCCESS;
                    } // Address
                } // INTERNAL_ElfContext_GetValueFromSymbolTable()
            } // INTERNAL_MMapModulePath()
        } // module->Base
    } // for()

    if( 0 == *Address )
    {
        retVal = ERROR_NOT_FOUND;
    } // Address

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

    INTERNAL_GetProcessMap_ProcSelfMaps();

    retVal = INTERNAL_ResolveSymbol( ImageName,
                                     SymbolName,
                                     Address );

    INTERNAL_ReleaseRuntimeModules();

    __DEBUG_RETVAL( retVal )
    return retVal;
}
