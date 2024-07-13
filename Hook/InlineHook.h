
#ifdef __cplusplus
extern "C" {
#endif

typedef void
    ( *CallBeforePageWrite )
    (
        uintptr_t   AlignedPageAddress
    );

typedef void
    ( *CallAfterPageWrite )
    (
        uintptr_t   AlignedPageAddress
    );

int
    BWSR_InlineHook
    (
        void*       Address,
        void*       HookFunction,
        void**      OutOriginalFunction,
        void*       BeforePageWriteFn,
        void*       AfterPageWriteFn
    );

int
    BWSR_DestroyHook
    (
        void*       Address
    );

void
    BWSR_DestroyAllHooks
    (
        void
    );

#ifdef __cplusplus
}
#endif