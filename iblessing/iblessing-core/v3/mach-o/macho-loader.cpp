//
//  macho-loader.cpp
//  iblessing-core
//
//  Created by soulghost on 2021/8/26.
//  Copyright © 2021 soulghost. All rights reserved.
//

#include "macho-loader.hpp"
#include "termcolor.h"
#include "StringUtils.h"
#include "ScannerContext.hpp"
#include "DyldSimulator.hpp"
#include <mach-o/loader.h>
#include <set>

#ifdef IB_PLATFORM_DARWIN
#include <filesystem>
#else
#include <experimental/filesystem>
#endif

#ifdef IB_PLATFORM_DARWIN
namespace fs = std::filesystem;
#else
namespace fs = std::experimental::filesystem;
#endif

using namespace std;
using namespace iblessing;

static string resolveLibraryPath(string &name) {
    // FIXME: @rpath
    string path;
    if (name.rfind("libc++") != string::npos) {
        StringUtils::replace(name, "libc++", "libcpp");
    }
    
    static const char *versions[] = { "A", "B", "C" };
    for (int i = 0; i < sizeof(versions) / sizeof(const char *); i++) {
        std::string versionPart = StringUtils::format("Versions/%s/", versions[i]);
        if (name.rfind(versionPart) != string::npos) {
            StringUtils::replace(name, versionPart, "");
        }
    }
    std::string libRoot = "/Users/soulghost/Desktop/git/iblessing/iblessing/resource/Frameworks/7.1";
    if (StringUtils::has_prefix(name, "/System/Library/Frameworks/")) {
        path = libRoot + name;
    } else if (StringUtils::has_prefix(name, "/usr/lib/")) {
        // FIXME: check file exists
        path = libRoot + name;
    }
    
    if (!filesystem::exists(path)) {
        cout << termcolor::yellow << "[-] MachOLoader - Warn: missing library " << name;
        cout << termcolor::reset << endl;
        return "";
    }
    return path;
}

MachoLoader::MachoLoader()  {
    loaderOffset = 0;
    
    workDirManager = new ScannerWorkDirManager("/tmp/iblessing-workdir");
    if (workDirManager->resetWorkDir() != 0) {
        assert(false);
    }
    
    // uc
    uc_err err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &this->uc);
    if (err) {
        cout << termcolor::red << "[-] MachOLoader - Error: unicorn error " << uc_strerror(err);
        cout << termcolor::reset << endl;
        assert(false);
    }
    // mach-o mapping start from 0x100000000 (app), 0x0 (dylib)
    // heap using vm_base ~ vmbase + 12G
    // stack using vmbase + 12G ~ .
    uint64_t unicorn_vm_size = 12UL * 1024 * 1024 * 1024;
    uint64_t unicorn_vm_start = 0x100000000;
    err = uc_mem_map(uc, unicorn_vm_start, unicorn_vm_size, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        cout << termcolor::red << "[-] MachOLoader - Error: unicorn error " << uc_strerror(err);
        cout << termcolor::reset << endl;
        assert(false);
    }
}

MachoLoader::~MachoLoader() {
    delete workDirManager;
}
 
shared_ptr<MachOModule> MachoLoader::loadModuleFromFile(std::string filePath) {
    assert(modules.size() == 0);
    shared_ptr<MachOModule> mainModule = _loadModuleFromFile(filePath, true);
    
    set<pair<string, string>> symbolNotFoundErrorSet;
    for (shared_ptr<MachOModule> module : modules) {
        DyldSimulator::eachBind(module->mappedBuffer, module->segmentHeaders, module->dyldInfoCommand, [&](uint64_t addr, uint8_t type, const char *symbolName, uint8_t symbolFlags, uint64_t addend, int64_t libraryOrdinal, const char *msg) {
            shared_ptr<MachOModule> targetModule = nullptr;
            if (libraryOrdinal <= 0) {
                switch (libraryOrdinal) {
                    case IB_BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE: {
                        assert(false);
                        break;
                    }
                    case IB_BIND_SPECIAL_DYLIB_SELF: {
                        targetModule = module;
                        break;
                    }
                    default: {
                        assert(false);
                    }
                }
            } else {
                if (libraryOrdinal - 1 >= module->dynamicLibraryOrdinalList.size()) {
                    cout << termcolor::yellow << StringUtils::format("[-] MachOLoader - Warn: eachBind error for %s, invalid libraryOrdinal %llu, total dylibs %lu", module->name.c_str(), libraryOrdinal, module->dynamicLibraryOrdinalList.size());
                    cout << termcolor::reset << endl;
                    return;
                }
                
                MachODynamicLibrary &library = module->dynamicLibraryOrdinalList[libraryOrdinal - 1];
                string libraryName = library.name;
                if (libraryName.rfind("libc++") != string::npos) {
                    StringUtils::replace(libraryName, "libc++", "libcpp");
                }
                if (name2module.find(libraryName) == name2module.end()) {
                    assert(false);
                }
                targetModule = name2module[libraryName];
            }
            assert(targetModule != nullptr);
            
            Symbol *sym = targetModule->symtab->getSymbolByName(symbolName);
            if (!sym) {
                pair<string, string> errorPattern = {symbolName, targetModule->name};
                if (symbolNotFoundErrorSet.find(errorPattern) == symbolNotFoundErrorSet.end()) {
                    cout << termcolor::yellow << StringUtils::format("[-] MachOLoader - Warn: eachBind cannot find symbol %s in %s\n", symbolName, targetModule->name.c_str());
                    cout << termcolor::reset << endl;
                    symbolNotFoundErrorSet.insert(errorPattern);
                }
                return;
            }
            assert(sym->info);
            assert(sym->info->n_value > 0);
            switch (type) {
                case IB_BIND_TYPE_POINTER: {
                    uint64_t symbolPtrAddr = sym->info->n_value + addend;
                    uint64_t symbolAddr = 0;
                    assert(uc_mem_read(uc, symbolPtrAddr, &symbolAddr, 8) == KERN_SUCCESS);
                    assert(uc_mem_write(uc, addr, &symbolAddr, 8) == KERN_SUCCESS);
                    printf("[+] bind %s(%s) at 0x%llx to 0x%llx(%s)\n", symbolName, targetModule->name.c_str(), symbolAddr, addr, module->name.c_str());
                    break;
                }
                case IB_BIND_TYPE_TEXT_ABSOLUTE32: {
                    uint64_t symbolAddr = sym->info->n_value + addend;
                    assert(uc_mem_write(uc, addr, &symbolAddr, 8) == KERN_SUCCESS);
                    printf("[+] bind %s(%s) at 0x%llx to 0x%llx(%s)\n", symbolName, targetModule->name.c_str(), symbolAddr, addr, module->name.c_str());
                    break;
                }
                default:
                    assert(false);
                    break;
            }
//            uint64_t symbolAddr = 0;
//            addr += module->addr;
//            uc_err err = uc_mem_read(uc, addr, &symbolAddr, 8);
//            if (err != UC_ERR_OK) {
//                cout << termcolor::yellow << StringUtils::format("[-] MachOLoader - Warn: eachBind error for %s, cannot read symbol from 0x%llx in %s", module->name.c_str(), addr, library.name.c_str());
//                cout << termcolor::reset << endl;
//            }
        });
    }
    return mainModule;
}

shared_ptr<MachOModule> MachoLoader::_loadModuleFromFile(std::string filePath, bool loadDylibs) {
    string moduleName = StringUtils::path_basename(filePath);
    if (name2module.find(moduleName) != name2module.end()) {
        return name2module[moduleName];
    }
    
    char *shadowFilePath = nullptr;
    if (workDirManager->createShadowFile(filePath, &shadowFilePath) != 0) {
        assert(false);
        return NULL;
    }
    
    if (shadowFilePath == nullptr) {
        assert(false);
        return NULL;
    }
    
    filePath = shadowFilePath;
    
    shared_ptr<MachOModule> module = make_shared<MachOModule>();
    module->name = moduleName;
    uint8_t *mappedFile;
    uint64_t bufferSize;
    ib_mach_header_64 *hdr = nullptr;
    scanner_err serr = ScannerContext::headerDetector(filePath, &mappedFile, &bufferSize, &hdr);
    if (serr != SC_ERR_OK) {
        assert(false);
        return NULL;
    }
    
    // parse section headers
    // vmaddr base
    uint64_t imageBase = loaderOffset;
    uint64_t imageSize = 0;
    vector<pair<uint64_t, uint64_t>> textSects;
    uint64_t vmaddr_bss_start = 0;
    uint64_t vmaddr_bss_end = 0;
    
    // offset, size, baseAddr, sect
    vector<pair<pair<uint64_t, uint64_t>, pair<uint64_t, ib_section_64 *>>> allRelocs;
    
    struct ib_symtab_command *symtab_cmd = nullptr;
    struct ib_dysymtab_command *dysymtab_cmd = nullptr;
    struct ib_segment_command_64 *textSeg64 = nullptr;
    struct ib_section_64 *textSect = nullptr;
    struct ib_entry_point_command *mainSeg = nullptr;
    struct ib_dyld_info_command *dyld_info = nullptr;
    uint64_t objc_classlist_addr = 0, objc_catlist_addr = 0;
    uint64_t objc_classlist_size = 0, objc_catlist_size = 0;
    std::vector<struct ib_section_64 *> sectionHeaders;
    std::vector<struct ib_segment_command_64 *> segmentHeaders;
    
    // symtab、dlsymtab、strtab's vmaddr base on LINKEDIT's vmaddr
    uint64_t linkedit_base = 0;
    uint64_t symoff = 0, symsize = 0;
    uint64_t stroff = 0, strsize = 0;
    uint32_t ncmds = hdr->ncmds;
    uint8_t *cmds = mappedFile + sizeof(struct ib_mach_header_64);
    
    std::vector<MachODynamicLibrary> dynamicLibraryDependencies;
    std::vector<MachODynamicLibrary> dynamicLibraryOrdinalList;
    std::vector<MachODynamicLibrary> exportDynamicLibraries;
    printf("[+] MachOLoader - load module %s (%s) with offset 0x%llx\n", moduleName.c_str(), filePath.c_str(), imageBase);
    for (uint32_t i = 0; i < ncmds; i++) {
        struct ib_load_command *lc = (struct ib_load_command *)cmds;
        switch (lc->cmd) {
            case IB_LC_SEGMENT_64: {
                struct ib_segment_command_64 *seg64 = (struct ib_segment_command_64 *)lc;
                segmentHeaders.push_back(seg64);
                
                uint64_t addr = seg64->vmaddr + imageBase;
                // update header
                seg64->vmaddr = addr;
                
                uint64_t size = std::min(seg64->vmsize, seg64->filesize);
                if (size == 0) {
                    cmds += lc->cmdsize;
                    continue;
                }
                uc_err err = uc_mem_write(uc, addr, mappedFile + seg64->fileoff, size);
                if (err != UC_ERR_OK) {
                    cout << termcolor::red << "[-] VirtualMemoryV2 - Error: cannot map segment ";
                    cout << termcolor::red << StringUtils::format("%s(0x%llx~0x%llx)",
                                                                  seg64->segname,
                                                                  seg64->vmaddr,
                                                                  seg64->vmaddr + size);
                    cout << ", error " << uc_strerror(err);
                    cout << termcolor::reset << endl;
                    assert(false);
                    return NULL;
                }
                printf("[+]   mapping %s: 0x%llx - 0x%llx\n", seg64->segname, addr, addr + size);
                
                if (strncmp(seg64->segname, "__TEXT", 6) == 0) {
                    textSeg64 = seg64;
                } else if (strncmp(seg64->segname, "__LINKEDIT", 10) == 0) {
                    linkedit_base = seg64->vmaddr - seg64->fileoff;
                }
                
                if (seg64->nsects > 0) {
                    struct ib_section_64 *sect = (struct ib_section_64 *)((uint8_t *)seg64 + sizeof(struct ib_segment_command_64));
                    for (uint32_t i = 0; i < seg64->nsects; i++) {
                        // update sect addr
                        sect->addr += imageBase;
                        
                        char *sectname = (char *)malloc(17);
                        memcpy(sectname, sect->sectname, 16);
                        sectname[16] = 0;
                        module->addr2segInfo[sect->addr] = {string(sect->segname), string(sectname)};
                        if (strcmp(sectname, "__text") == 0) {
                            textSects.push_back({sect->addr, sect->size});
                            textSect = sect;
                        }
                        if (strcmp(sectname, "__bss") == 0) {
                            vmaddr_bss_start = sect->addr;
                            vmaddr_bss_end = vmaddr_bss_start + sect->size;
                        }
                        if (strcmp(sectname, "__objc_classlist") == 0) {
                            objc_classlist_addr = sect->addr;
                            objc_classlist_size = sect->size;
                        }
                        if (strcmp(sectname, "__objc_catlist") == 0) {
                            objc_catlist_addr = sect->addr;
                            objc_catlist_size = sect->size;
                        }
                        
                        if (sect->reloff > 0 && sect->nreloc > 0) {
                            allRelocs.push_back({{sect->reloff, sect->nreloc}, {sect->addr, sect}});
                        }
                        
                        uint64_t addr = sect->addr;
                        uc_err err = uc_mem_write(uc, addr, mappedFile + sect->offset, sect->size);
                        if (err != UC_ERR_OK) {
                            cout << termcolor::red << "[-] VirtualMemoryV2 - Error: cannot map section ";
                            cout << StringUtils::format("%s(0x%llx~0x%llx)",
                                                        sect->segname,
                                                        sect->addr,
                                                        addr + sect->size);
                            cout << ", error " << uc_strerror(err);
                            cout << termcolor::reset << endl;
                            assert(false);
                        }
                        printf("[+]     mapping %s.%s: 0x%llx - 0x%llx\n", sect->segname, sectname, addr, addr + size);
                        sectionHeaders.push_back(sect);
                        free(sectname);
                        sect += 1;
                    }
                }
                
                // update size
                uint64_t totalSize = seg64->vmaddr + seg64->vmsize - imageBase;
                if (imageSize < totalSize) {
                    imageSize = totalSize;
                }
                break;
            }
            case IB_LC_SYMTAB: {
                symtab_cmd = (struct ib_symtab_command *)lc;
                symoff = symtab_cmd->symoff;
                symsize = symtab_cmd->nsyms * sizeof(ib_nlist_64);
                stroff = symtab_cmd->stroff;
                strsize = symtab_cmd->strsize;
                break;
            }
            case IB_LC_DYSYMTAB: {
                dysymtab_cmd = (struct ib_dysymtab_command *)lc;
                break;
            }
            case IB_LC_DYLD_INFO_ONLY: {
                dyld_info = (struct ib_dyld_info_command *)lc;
                break;
            }
            case IB_LC_MAIN: {
                struct ib_entry_point_command *lc_main = (struct ib_entry_point_command *)lc;
                mainSeg = lc_main;
                break;
            }
            case IB_LC_LOAD_DYLIB: {
                struct ib_dylib_command *dylib_cmd = (struct ib_dylib_command *)lc;
                const char *path = (const char *)dylib_cmd + dylib_cmd->dylib.name.offset;
                string name = StringUtils::path_basename(std::string(path));
                dynamicLibraryDependencies.push_back({.name = name, .path = std::string(path), .upward = false, .weak = false});
                dynamicLibraryOrdinalList.push_back({.name = name, .path = std::string(path), .upward = false, .weak = false});
                break;
            }
            case IB_LC_LOAD_WEAK_DYLIB: {
                struct ib_dylib_command *dylib_cmd = (struct ib_dylib_command *)lc;
                const char *path = (const char *)dylib_cmd + dylib_cmd->dylib.name.offset;
                string name = StringUtils::path_basename(std::string(path));
                dynamicLibraryDependencies.push_back({.name = name, .path = std::string(path), .upward = false, .weak = true});
                dynamicLibraryOrdinalList.push_back({.name = name, .path = std::string(path), .upward = false, .weak = true});
                break;
            }
            case IB_LC_REEXPORT_DYLIB: {
                struct ib_dylib_command *dylib_cmd = (struct ib_dylib_command *)lc;
                const char *path = (const char *)dylib_cmd + dylib_cmd->dylib.name.offset;
                string name = StringUtils::path_basename(std::string(path));
                dynamicLibraryOrdinalList.push_back({.name = name, .path = std::string(path), .upward = false, .weak = false});
                exportDynamicLibraries.push_back({.name = name, .path = std::string(path), .upward = false, .weak = false});
                break;
            }
            case IB_LC_LAZY_LOAD_DYLIB: {
                struct ib_dylib_command *dylib_cmd = (struct ib_dylib_command *)lc;
                const char *path = (const char *)dylib_cmd + dylib_cmd->dylib.name.offset;
                string name = StringUtils::path_basename(std::string(path));
                dynamicLibraryOrdinalList.push_back({.name = name, .path = std::string(path), .upward = false, .weak = false});
                break;
            }
            case IB_LC_LOAD_UPWARD_DYLIB: {
                struct ib_dylib_command *dylib_cmd = (struct ib_dylib_command *)lc;
                const char *path = (const char *)dylib_cmd + dylib_cmd->dylib.name.offset;
                string name = StringUtils::path_basename(std::string(path));
                dynamicLibraryDependencies.push_back({.name = name, .path = std::string(path), .upward = true, .weak = false});
                dynamicLibraryOrdinalList.push_back({.name = name, .path = std::string(path), .upward = true, .weak = false});
                break;
            }
            default:
                break;
        }
        cmds += lc->cmdsize;
    }
    
    shared_ptr<StringTable> strtab = make_shared<StringTable>();
    module->strtab = strtab;
    uint64_t strtab_vmaddr = linkedit_base + symtab_cmd->stroff;
    strtab->buildStringTable(strtab_vmaddr, mappedFile + symtab_cmd->stroff, symtab_cmd->strsize);
    
    // sort sectionHeaders by offset
    sort(sectionHeaders.begin(), sectionHeaders.end(), [&](struct ib_section_64 *a, struct ib_section_64 *b) {
        return a->offset < b->offset;
    });
    shared_ptr<SymbolTable> symtab = make_shared<SymbolTable>(strtab);
    symtab->moduleBase = imageBase;
    module->symtab = symtab;
    symtab->buildSymbolTable(mappedFile + symtab_cmd->symoff, symtab_cmd->nsyms);
    if (dysymtab_cmd) {
        symtab->buildDynamicSymbolTable(sectionHeaders, mappedFile + dysymtab_cmd->indirectsymoff, dysymtab_cmd->nindirectsyms, mappedFile);
    }
    symtab->sync();
    
    // map symtab & strtab
    uc_err err = uc_mem_write(uc, linkedit_base + symoff, mappedFile + symoff, symsize);
    if (err != UC_ERR_OK) {
        cout << termcolor::red << "[-] VirtualMemoryV2 - Error: cannot map symbol table: " << uc_strerror(err);
        cout << termcolor::reset << endl;
        assert(false);
        return NULL;
    }
    
    err = uc_mem_write(uc, linkedit_base + stroff, mappedFile + stroff, strsize);
    if (err != UC_ERR_OK) {
        cout << termcolor::red << "[-] VirtualMemoryV2 - Error: cannot map string table: " << uc_strerror(err);
        cout << termcolor::reset << endl;
        assert(false);
        return NULL;
    }
    
//    if (uc != this->uc) {
//        // sync text segment since we may have fixed it
//        for (pair<uint64_t, uint32_t> patch : textPatch) {
//            uc_mem_write(uc, patch.first, &patch.second, sizeof(uint32_t));
//        }
//        relocAllRegions(symtab, objcRuntime, uc);
//    }
    
    // handle export dylibs
    for (MachODynamicLibrary &library : exportDynamicLibraries) {
        string path = resolveLibraryPath(library.path);
        if (path.length() > 0) {
            _loadModuleFromFile(path, false);
        } else {
            cout << termcolor::yellow << StringUtils::format("[-] MachOLoader - Error: unable to export dependent dylib %s", library.name.c_str());
            cout << termcolor::reset << endl;
        }
    }
    
    module->addr = imageBase;
    module->size = imageSize;
    module->dynamicLibraryDependencies = dynamicLibraryDependencies;
    module->dynamicLibraryOrdinalList = dynamicLibraryOrdinalList;
    module->exportDynamicLibraries = exportDynamicLibraries;
    module->dyldInfoCommand = dyld_info;
    module->mappedBuffer = mappedFile;
    module->segmentHeaders = segmentHeaders;
    
    modules.push_back(module);
    assert(name2module.find(moduleName) == name2module.end());
    name2module[moduleName] = module;
    
    // rebase module
    if (imageBase > 0) {
        DyldSimulator::doRebase(imageBase, imageSize, mappedFile, segmentHeaders, dyld_info, [&](uint64_t addr, uint64_t slide, uint8_t type) {
            switch (type) {
                case IB_REBASE_TYPE_POINTER:
                case IB_REBASE_TYPE_TEXT_ABSOLUTE32: {
                    uint64_t ptrAddr = addr;
                    uint64_t ptrValue = 0;
                    uc_err err = uc_mem_read(uc, ptrAddr, &ptrValue, 8);
                    if (err != UC_ERR_OK) {
                        cout << termcolor::red << StringUtils::format("[-] MachOLoader - Error: cannot do rebase at 0x%llx, %s", addr, uc_strerror(err));
                        cout << termcolor::reset << endl;
                        assert(false);
                    }
                    
                    ptrValue += imageBase;
                    err = uc_mem_write(uc, ptrAddr, &ptrValue, 8);
                    if (err != UC_ERR_OK) {
                        cout << termcolor::red << StringUtils::format("[-] MachOLoader - Error: cannot do rebase at 0x%llx, %s", addr, uc_strerror(err));
                        cout << termcolor::reset << endl;
                        assert(false);
                    }
                    break;
                }
                default:
                    assert(false);
                    break;
            }
        });
    }
    
    
    loaderOffset += imageSize;
    
    // load dependencies
    if (loadDylibs) {
        for (MachODynamicLibrary &library : dynamicLibraryDependencies) {
            string path = resolveLibraryPath(library.path);
            if (path.length() != 0) {
                _loadModuleFromFile(path, true);
            } else {
                cout << termcolor::yellow << StringUtils::format("[-] MachOLoader - Error: unable to load dependent dylib %s", library.path.c_str());
                cout << termcolor::reset << endl;
            }
        }
    }
    
    return module;
}