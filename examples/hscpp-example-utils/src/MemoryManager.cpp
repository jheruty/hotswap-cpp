#include <assert.h>

#include "hscpp-example-utils/MemoryManager.h"
#include "hscpp/Hotswapper.h"

uint8_t* MemoryManager::GetMemory(uint64_t id)
{
    if (id < m_Blocks.size())
    {
        return m_Blocks.at(static_cast<size_t>(id)).pMemory;
    }
    else if (id == MEMORY_MANAGER_ID)
    {
        return reinterpret_cast<uint8_t*>(this);
    }

    return nullptr;
}

hscpp::AllocationInfo MemoryManager::Hscpp_Allocate(uint64_t size)
{
    size_t iBlock = TakeFirstFreeBlock(size);

    hscpp::AllocationInfo info;
    info.id = static_cast<uint64_t>(iBlock);
    info.pMemory = m_Blocks.at(iBlock).pMemory;

    return info;
}

hscpp::AllocationInfo MemoryManager::Hscpp_AllocateSwap(uint64_t previousId, uint64_t size)
{
    if (previousId < m_Blocks.size())
    {
        Block& block = m_Blocks.at(static_cast<size_t>(previousId));

        block.bFree = false;
        block.capacity = size;
        block.pMemory = new uint8_t[static_cast<size_t>(size)];

        hscpp::AllocationInfo info;
        info.id = previousId;
        info.pMemory = block.pMemory;

        return info;
    }

    assert(false);
    return hscpp::AllocationInfo();
}

uint64_t MemoryManager::Hscpp_Free(uint8_t* pMemory)
{
    for (size_t i = 0; i < m_Blocks.size(); ++i)
    {
        Block& block = m_Blocks.at(i);
        if (pMemory == block.pMemory)
        {
            block.bFree = true;
            block.capacity = 0;
            delete[] block.pMemory; // Delete the memory to cause obvious faults.
            
            return i;
        }
    }

    assert(false);
    return 0;
}

size_t MemoryManager::TakeFirstFreeBlock(uint64_t size)
{
    size_t iBlock = (std::numeric_limits<size_t>::max)();
    for (size_t i = 0; i < m_Blocks.size(); ++i)
    {
        Block& block = m_Blocks.at(i);

        if (block.bFree && !block.bExternallyOwned && size < block.capacity)
        {
            iBlock = i;
            break;
        }
    }

    if (iBlock == (std::numeric_limits<size_t>::max)())
    {
        iBlock = m_Blocks.size();

        Block block;
        block.bFree = false;
        block.capacity = size;
        block.pMemory = new uint8_t[static_cast<size_t>(size)];

        m_Blocks.push_back(block);
    }

    return iBlock;
}
