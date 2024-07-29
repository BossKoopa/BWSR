#ifndef __ELF_H__
#define __ELF_H__

int
    BWSR_ResolveSymbol
    (
        const char*             SymbolName,
        const char*             ImageName,
        uintptr_t*              Address
    );

#endif // __MACHO_H__