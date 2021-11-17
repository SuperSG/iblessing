//
//  dyld_cache_format.h
//  dyld_cache_format
//
//  Created by Soulghost on 2021/11/14.
//  Copyright © 2021 soulghost. All rights reserved.
//

#ifndef dyld_cache_format_h
#define dyld_cache_format_h

#include <iblessing-core/v2/common/ibtypes.h>
#include <iblessing-core/core/polyfill/mach-universal.hpp>

struct dyld_cache_header
{
    char        magic[16];              // e.g. "dyld_v0    i386"
    uint32_t    mappingOffset;          // file offset to first dyld_cache_mapping_info
    uint32_t    mappingCount;           // number of dyld_cache_mapping_info entries
    uint32_t    imagesOffset;           // file offset to first dyld_cache_image_info
    uint32_t    imagesCount;            // number of dyld_cache_image_info entries
    uint64_t    dyldBaseAddress;        // base address of dyld when cache was built
    uint64_t    codeSignatureOffset;    // file offset of code signature blob
    uint64_t    codeSignatureSize;      // size of code signature blob (zero means to end of file)
    uint64_t    slideInfoOffsetUnused;  // unused.  Used to be file offset of kernel slid info
    uint64_t    slideInfoSizeUnused;    // unused.  Used to be size of kernel slid info
    uint64_t    localSymbolsOffset;     // file offset of where local symbols are stored
    uint64_t    localSymbolsSize;       // size of local symbols information
    uint8_t     uuid[16];               // unique value for each shared cache file
    uint64_t    cacheType;              // 0 for development, 1 for production
    uint32_t    branchPoolsOffset;      // file offset to table of uint64_t pool addresses
    uint32_t    branchPoolsCount;       // number of uint64_t entries
    uint64_t    accelerateInfoAddr;     // (unslid) address of optimization info
    uint64_t    accelerateInfoSize;     // size of optimization info
    uint64_t    imagesTextOffset;       // file offset to first dyld_cache_image_text_info
    uint64_t    imagesTextCount;        // number of dyld_cache_image_text_info entries
    uint64_t    patchInfoAddr;          // (unslid) address of dyld_cache_patch_info
    uint64_t    patchInfoSize;          // Size of all of the patch information pointed to via the dyld_cache_patch_info
    uint64_t    otherImageGroupAddrUnused;    // unused
    uint64_t    otherImageGroupSizeUnused;    // unused
    uint64_t    progClosuresAddr;       // (unslid) address of list of program launch closures
    uint64_t    progClosuresSize;       // size of list of program launch closures
    uint64_t    progClosuresTrieAddr;   // (unslid) address of trie of indexes into program launch closures
    uint64_t    progClosuresTrieSize;   // size of trie of indexes into program launch closures
    uint32_t    platform;               // platform number (macOS=1, etc)
    uint32_t    formatVersion          : 8,  // dyld3::closure::kFormatVersion
                dylibsExpectedOnDisk   : 1,  // dyld should expect the dylib exists on disk and to compare inode/mtime to see if cache is valid
                simulator              : 1,  // for simulator of specified platform
                locallyBuiltCache      : 1,  // 0 for B&I built cache, 1 for locally built cache
                builtFromChainedFixups : 1,  // some dylib in cache was built using chained fixups, so patch tables must be used for overrides
                padding                : 20; // TBD
    uint64_t    sharedRegionStart;      // base load address of cache if not slid
    uint64_t    sharedRegionSize;       // overall size of region cache can be mapped into
    uint64_t    maxSlide;               // runtime slide of cache can be between zero and this value
    uint64_t    dylibsImageArrayAddr;   // (unslid) address of ImageArray for dylibs in this cache
    uint64_t    dylibsImageArraySize;   // size of ImageArray for dylibs in this cache
    uint64_t    dylibsTrieAddr;         // (unslid) address of trie of indexes of all cached dylibs
    uint64_t    dylibsTrieSize;         // size of trie of cached dylib paths
    uint64_t    otherImageArrayAddr;    // (unslid) address of ImageArray for dylibs and bundles with dlopen closures
    uint64_t    otherImageArraySize;    // size of ImageArray for dylibs and bundles with dlopen closures
    uint64_t    otherTrieAddr;          // (unslid) address of trie of indexes of all dylibs and bundles with dlopen closures
    uint64_t    otherTrieSize;          // size of trie of dylibs and bundles with dlopen closures
    uint32_t    mappingWithSlideOffset; // file offset to first dyld_cache_mapping_and_slide_info
    uint32_t    mappingWithSlideCount;  // number of dyld_cache_mapping_and_slide_info entries
};

struct dyld_cache_mapping_info {
    uint64_t    address;
    uint64_t    size;
    uint64_t    fileOffset;
    uint32_t    maxProt;
    uint32_t    initProt;
};

struct dyld_cache_mapping_and_slide_info {
    uint64_t    address;
    uint64_t    size;
    uint64_t    fileOffset;
    uint64_t    slideInfoFileOffset;
    uint64_t    slideInfoFileSize;
    uint64_t    flags;
    uint32_t    maxProt;
    uint32_t    initProt;
};

struct dyld_cache_image_info
{
    uint64_t    address;
    uint64_t    modTime;
    uint64_t    inode;
    uint32_t    pathFileOffset;
    uint32_t    pad;
};

enum {
    DYLD_CACHE_MAPPING_AUTH_DATA            = 1 << 0U,
    DYLD_CACHE_MAPPING_DIRTY_DATA           = 1 << 1U,
    DYLD_CACHE_MAPPING_CONST_DATA           = 1 << 2U,
};

struct dyld_cache_slide_info2
{
    uint32_t    version;            // currently 2
    uint32_t    page_size;          // currently 4096 (may also be 16384)
    uint32_t    page_starts_offset;
    uint32_t    page_starts_count;
    uint32_t    page_extras_offset;
    uint32_t    page_extras_count;
    uint64_t    delta_mask;         // which (contiguous) set of bits contains the delta to the next rebase location
    uint64_t    value_add;
    //uint16_t    page_starts[page_starts_count];
    //uint16_t    page_extras[page_extras_count];
};

#endif /* dyld_cache_format_h */