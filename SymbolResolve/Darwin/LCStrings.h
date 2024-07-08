
#ifndef __LCSTRINGS_H__
#define __LCSTRINGS_H__

#define LOAD_COMMAND_STRINGS( E ) \
    E( LC_SEGMENT,	                    "segment of this file to be mapped"         )   \
    E( LC_SYMTAB,	                    "link-edit stab symbol table info"          )   \
    E( LC_SYMSEG,	                    "link-edit gdb symbol table info "              \
                                        "(obsolete)"                                )   \
    E( LC_THREAD,	                    "thread"                                    )   \
    E( LC_UNIXTHREAD,	                "unix thread (includes a stack)"            )   \
    E( LC_LOADFVMLIB,	                "load a specified fixed VM shared library"  )   \
    E( LC_IDFVMLIB,	                    "fixed VM shared library identification"    )   \
    E( LC_IDENT,	                    "object identification info (obsolete)"     )   \
    E( LC_FVMFILE,	                    "fixed VM file inclusion (internal use)"    )   \
    E( LC_PREPAGE,                      "prepage command (internal use)"            )   \
    E( LC_DYSYMTAB,	                    "dynamic link-edit symbol table info"       )   \
    E( LC_LOAD_DYLIB,	                "load a dynamically linked shared library"  )   \
    E( LC_ID_DYLIB,	                    "dynamically linked shared lib ident"       )   \
    E( LC_LOAD_DYLINKER,	            "load a dynamic linker"                     )   \
    E( LC_ID_DYLINKER,	                "dynamic linker identification"             )   \
    E( LC_PREBOUND_DYLIB, 	            "modules prebound for a dynamically "           \
                                        "linked shared library"                     )   \
    E( LC_ROUTINES,		                "image routines"                            )   \
    E( LC_SUB_FRAMEWORK, 	            "sub framework"                             )   \
    E( LC_SUB_UMBRELLA, 	            "sub umbrella"                              )   \
    E( LC_SUB_CLIENT,		            "sub client"                                )   \
    E( LC_SUB_LIBRARY,  	            "sub library"                               )   \
    E( LC_TWOLEVEL_HINTS, 	            "two-level namespace lookup hints"          )   \
    E( LC_PREBIND_CKSUM,  	            "prebind checksum"                          )   \
    E( LC_LOAD_WEAK_DYLIB,              "load a dynamically linked shared library "     \
                                        "that is allowed to be missing (all "           \
                                        "symbols are weak imported)."               )   \
    E( LC_SEGMENT_64, 	                "64-bit segment of this file to be mapped"  )   \
    E( LC_ROUTINES_64,		            "64-bit image routines"                     )   \
    E( LC_UUID,			                "the uuid"                                  )   \
    E( LC_RPATH,                        "runpath additions"                         )   \
    E( LC_CODE_SIGNATURE, 	            "local of code signature"                   )   \
    E( LC_SEGMENT_SPLIT_INFO,           "local of info to split segments"           )   \
    E( LC_REEXPORT_DYLIB,               "load and re-export dylib"                  )   \
    E( LC_LAZY_LOAD_DYLIB, 	            "delay load of dylib until first use"       )   \
    E( LC_ENCRYPTION_INFO, 	            "encrypted segment information"             )   \
    E( LC_DYLD_INFO, 		            "compressed dyld information"               )   \
    E( LC_DYLD_INFO_ONLY, 	            "compressed dyld information only"          )   \
    E( LC_LOAD_UPWARD_DYLIB,            "load upward dylib"                         )   \
    E( LC_VERSION_MIN_MACOSX,           "build for MacOSX min OS version"           )   \
    E( LC_VERSION_MIN_IPHONEOS,         "build for iPhoneOS min OS version"         )   \
    E( LC_FUNCTION_STARTS,              "compressed table of function start "           \
                                        "addresses"                                 )   \
    E( LC_DYLD_ENVIRONMENT,             "string for dyld to treat like "                \
                                        "environment variable"                      )   \
    E( LC_MAIN,                         "replacement for LC_UNIXTHREAD"             )   \
    E( LC_DATA_IN_CODE,                 "table of non-instructions in __text"       )   \
    E( LC_SOURCE_VERSION,               "source version used to build binary"       )   \
    E( LC_DYLIB_CODE_SIGN_DRS,          "Code signing DRs copied from linked "          \
                                        "dylibs"                                    )   \
    E( LC_ENCRYPTION_INFO_64,           "64-bit encrypted segment information"      )   \
    E( LC_LINKER_OPTION,                "linker options in MH_OBJECT files"         )   \
    E( LC_LINKER_OPTIMIZATION_HINT,     "optimization hints in MH_OBJECT files"     )   \
    E( LC_VERSION_MIN_TVOS,             "build for AppleTV min OS version"          )   \
    E( LC_VERSION_MIN_WATCHOS,          "build for Watch min OS version"            )   \
    E( LC_NOTE,                         "arbitrary data included within a Mach-O "      \
                                        "file"                                      )   \
    E( LC_BUILD_VERSION,                "build for platform min OS version"         )   \
    E( LC_DYLD_EXPORTS_TRIE,            "used with linkedit_data_command, payload "     \
                                        "is trie"                                   )   \
    E( LC_DYLD_CHAINED_FIXUPS,          "used with linkedit_data_command"           )   \
    E( LC_FILESET_ENTRY,                "used with fileset_entry_command"           )   \
    E( LC_ATOM_INFO,                    "used with linkedit_data_command"           )

#define LOAD_COMMAND_TO_TEXT( LOAD_COMMAND, TEXT ) \
    case LOAD_COMMAND: return #LOAD_COMMAND " (" TEXT ")";

static inline const char* LoadCommandString( int LoadCommand )
{
    switch( LoadCommand )
    {
        LOAD_COMMAND_STRINGS( LOAD_COMMAND_TO_TEXT )
    }

    return "Unknown Load Command!";
}

#endif __LCSTRINGS_H__