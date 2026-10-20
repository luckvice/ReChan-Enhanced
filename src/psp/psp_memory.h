#pragma once

#include <cstdlib>

#if defined(RC_PLATFORM_PSP)
#include <pspsysmem.h>
#endif

// Whether this run has the large PSP-2000+ user heap (MEMSIZE=1 EBOOT, about
// 45 MB) rather than the ~17.5 MB a PSPLink PRX gets or the 24 MB PSP-1000 limit.
// Data that is optional but avoids Memory Stick reads during gameplay (resident
// sound banks, voice clip cache) is only kept in the large heap.
// On PSP, we query the OS kernel partition directly for the largest free block
// (sceKernelMaxFreeMemSize) rather than doing a speculative malloc(24MB) that
// permanently expands the C runtime heap arena (sbrk) on boot.
inline bool PspHasLargeHeap() {
    static int cached = -1;
    if (cached < 0) {
#if defined(RC_PLATFORM_PSP)
        const SceSize maxFree = sceKernelMaxFreeMemSize();
        cached = (maxFree >= 24u * 1024u * 1024u) ? 1 : 0;
#else
        void* probe = std::malloc(24u * 1024u * 1024u);
        cached = probe != nullptr ? 1 : 0;
        std::free(probe);
#endif
    }
    return cached != 0;
}

