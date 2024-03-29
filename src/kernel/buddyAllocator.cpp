#include "../../h/buddyAllocator.hpp"
#include "../../h/KConsole.hpp"

#define BLOCK_SIZE 4096

static buddyAllocator* buddy = nullptr;

size_t getBlockAddr(size_t addr)
{
    size_t mask = ((size_t)-1) << BLOCK_SIZE_POWER_2;
    return addr & mask;
}
size_t getNextBlockAddr(size_t addr)
{
    size_t currBlock = getBlockAddr(addr);
    return currBlock + BLOCK_SIZE;
}

size_t getDeg2Floor(size_t num)
{
    size_t x = 1;
    size_t deg = 0;
    while(x <= num)
    {
        deg++;
        x<<=1;
    }
    return deg - 1;
}

size_t getDeg2Ceil(size_t num)
{
    size_t x = 1;
    size_t deg = 0;
    while(x < num)
    {
        deg++;
        x<<=1;
    }
    return deg;
}

//adds free block to level
void addBlockToLevel(void* addr, size_t level)
{
    buddyBlock* newBlock = (buddyBlock*)addr;
    newBlock->next = nullptr;
    if(buddy->bucket[level].first == nullptr)
    {
        buddy->bucket[level].first = buddy->bucket[level].last = newBlock;
    }
    else
    {
        buddy->bucket[level].last->next = newBlock;
        buddy->bucket[level].last = newBlock;
    }
}

size_t getBuddyBlockAddr(void* addr, size_t level)
{
    size_t diff = BLOCK_SIZE << level;
    size_t off = (size_t)addr - (size_t)buddy->startAddr;
    if(diff & off)
        return (size_t)addr - diff;
    else
        return (size_t)addr + diff;
}

//function to add free blocks to the buddy
void addBlocks(void* addr, size_t level)
{
    if(level == buddy->maxLevel)
    {
        addBlockToLevel(addr, level);
        return;
    }
    size_t buddyBlockAddr = getBuddyBlockAddr(addr,level);
    buddyBlock* curr = buddy->bucket[level].first;
    buddyBlock* prev = nullptr;
    while(curr != nullptr)
    {
        if((size_t)curr == buddyBlockAddr)
        {
            if(prev != nullptr)
            {
                prev->next = curr->next;
                if(curr == buddy->bucket[level].last)
                    buddy->bucket[level].last = prev;
            }
            else
            {
                if(curr->next == nullptr)
                    buddy->bucket[level].first = buddy->bucket[level].last = nullptr;
                else
                    buddy->bucket[level].first = curr->next;
            }
            if((size_t)addr < buddyBlockAddr) addBlocks(addr, level + 1); //merge two chunks
            else addBlocks((void*)buddyBlockAddr, level + 1); //merge two chunks
            return;
        }

        prev = curr;
        curr = curr->next;
    }

    addBlockToLevel(addr, level);
}

void printBuddyInfo()
{
    KConsole::trapPrintString("Buddy info-------------------------------------------\n");
    KConsole::trapPrintStringInt("Buddy starting address ", (size_t)buddy->startAddr, 16);
    KConsole::trapPrintStringInt("Buddy number of blocks ", buddy->numOfBlocks);
    KConsole::trapPrintStringInt("Buddy number of free blocks ", buddy->numOfFreeBlocks);
    for(int i = buddy->maxLevel;i>=0;i--)
    {
        KConsole::trapPrintStringInt("Level ",i);
        KConsole::trapPrintString("Free blocks on this level\n");
        buddyBlock* curr = buddy->bucket[i].first;
        while(curr != 0)
        {
            KConsole::trapPrintInt((size_t)curr, 16);
            KConsole::trapPrintString("\n");
            curr = curr->next;
        }
    }
    KConsole::trapPrintString("End of buddy info-------------------------------------------\n");
}

void* getBuddy()
{
    return (void*)buddy;
}

//split chunks into smaller chunks, until it is necessary
void split(void* addr, size_t finalLevel, size_t currLevel, bool splitMore)
{
    if(!splitMore)
    {
        addBlockToLevel(addr, currLevel);
    }
    else
    {
        if(currLevel == finalLevel)
            return;
        size_t buddyBlock = getBuddyBlockAddr(addr, currLevel - 1); // currLevel - 1 for next step
        split((void*)buddyBlock, finalLevel, currLevel - 1, false); // continue splitting
        split(addr, finalLevel, currLevel - 1, true); // continue splitting
    }
}
//--------------------------------------------------------------------------------------------
//buddy public interface

buddyAllocator* buddy_init(void* addr, size_t numOfBlocks)
{
    size_t blockAddr = getBlockAddr((size_t)addr); //alling the address
    size_t buddyAddr = (size_t)addr;
    if((size_t)addr != blockAddr) // if start address not aligned
    {
        buddyAddr = getNextBlockAddr(blockAddr);
        numOfBlocks--; //have to discard one block
    }
    numOfBlocks--; // take one block for buddy metadata
    buddy = (buddyAllocator*)buddyAddr;
    buddy->startAddr = (void*)getNextBlockAddr(buddyAddr);
    buddy->numOfBlocks = numOfBlocks;
    buddy->numOfFreeBlocks = numOfBlocks;
    buddy->maxLevel = getDeg2Floor(numOfBlocks);

    for(size_t i = 0; i <= buddy->maxLevel;i++)
        buddy->bucket[i].first = buddy->bucket[i].last = nullptr;

    for(size_t i = 0;i < numOfBlocks;i++)
        addBlocks((block*)buddy->startAddr + i, 0);

    return buddy;
}

void* buddy_alloc(size_t numOfBlocks)
{
    size_t level = getDeg2Ceil(numOfBlocks);
    for(size_t i = level; i <= buddy->maxLevel;i++)
    {
        if(buddy->bucket[i].first != nullptr)
        {
            buddyBlock* ret = buddy->bucket[i].first;
            buddy->bucket[i].first = ret->next;
            if(buddy->bucket[i].first == nullptr)
                buddy->bucket[i].last = nullptr;
            split((void*)ret, level, i, true);
            buddy->numOfFreeBlocks-= (1 << level);
            return ret;
        }
    }

    return nullptr;
}

void buddy_free(void* addr, size_t numOfBlocks)
{
    size_t level = getDeg2Ceil(numOfBlocks);
    addBlocks(addr, level); //adds free blocks
    buddy->numOfFreeBlocks += (1 << level);
}
