
#ifndef __MACHO_H__
#define __MACHO_H__

int
    BWSR_ResolveSymbol
    (
        const char*             SymbolName,
        const char*             ImageName,
        uintptr_t*              Address
    );

#endif // __MACHO_H__
