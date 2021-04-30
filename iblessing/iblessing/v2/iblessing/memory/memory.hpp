//
//  memory.hpp
//  iblessing
//
//  Created by soulghost on 2021/4/30.
//  Copyright © 2021 soulghost. All rights reserved.
//

#ifndef memory_hpp
#define memory_hpp

#include <iblessing/mach-o/mach-o.hpp>
#include "VirtualMemory.hpp"
#include "VirtualMemoryV2.hpp"

namespace iblessing {

class Memory {
public:
    Memory(std::shared_ptr<MachO> macho) : macho(macho) {}

    static std::shared_ptr<Memory> createFromMachO(std::shared_ptr<MachO> macho);
    ib_return_t loadSync();
    ib_return_t copyToUCEngine(uc_engine *uc);
    
    std::shared_ptr<VirtualMemory> fileMemory;
    std::shared_ptr<VirtualMemoryV2> virtualMemory;
    
protected:
    std::shared_ptr<MachO> macho;
};

};

#endif /* memory_hpp */
