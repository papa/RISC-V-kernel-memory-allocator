//
// Created by os on 4/27/22.
//

#include "../../h/PCB.hpp"
#include "../../h/syscall_c.hpp"
#include "../../h/SleepPCBList.hpp"
#include "../user/test/printing.hpp"
#include "../../h/KConsole.hpp"
#include "../../h/Scheduler.hpp"
#include "../../h/Riscv.hpp"
#include "../../h/syscall_c_kernel.hpp"
#include "../../h/PCBWrapperUser.hpp"

PCB* PCB::running = 0;
uint64 PCB::timeSliceCounter = 0;
uint64 PCB::savedRegA4 = 0;
PCB* PCB::consolePCB = 0;
PCB* PCB::userPCB = 0;

PCB::PCB(Body body, void *args, void* stack_space, uint64 timeSlice) :
    timeSlice(timeSlice),
    body(body),
    args(args),
    beginSP(stack_space),
    context({
        (uint64)((char*)stack_space + DEFAULT_STACK_SIZE),
        (uint64)&PCB::runner
    })
{
    nextPCB = 0;
}

void PCB::start()
{
    Scheduler::put(this);
}

void PCB::runner()
{
    Riscv::popSppSpie();
    running->body(running->args);

    thread_exit_kernel();
}

void PCB::dispatch()
{
    PCB* old = running;
    if(old->getState() == PCB::RUNNING)
        Scheduler::put(old);

    PCB::running = Scheduler::get();
    PCB::running->setState(PCB::RUNNING);

    Riscv::changePrivMode();

    PCB::contextSwitch(&old->context, &running->context);
}

void *PCB::operator new(size_t size) {
    return kmem_cache_alloc(Riscv::pcbCache);
}

void PCB::operator delete(void *p) {
    kmem_cache_free(Riscv::pcbCache, p);
}

PCB::~PCB()
{
    MemoryAllocator::kfree(beginSP);
}

void PCB::initialize()
{
    PCB* mainSystem = new PCB(0, 0, 0, 0);
    mainSystem->systemThread = true;
    PCB::running = mainSystem;
    PCB::running->setState(PCB::RUNNING);
    PCB::consolePCB = new PCB(&KConsole::sendCharactersToConsole, 0,
                              kmalloc(DEFAULT_STACK_SIZE),
                              DEFAULT_TIME_SLICE);
    PCB::consolePCB->systemThread = true;
    PCB::consolePCB->start();
    PCB::userPCB = new PCB(&PCBWrapperUser::userMainWrapper, 0,
                           MemoryAllocator::kmalloc(DEFAULT_STACK_SIZE),
                           DEFAULT_TIME_SLICE);
    PCB::userPCB->start();
}

bool PCB::isFinished()
{
    return state == PCB::FINISHED;
}

void PCB::threadExitHandler()
{
    PCB::timeSliceCounter = 0;
    PCB::running->setState(PCB::FINISHED);
    PCB::dispatch();
    __asm__ volatile("li a0, 0");
    Riscv::w_a0_sscratch();
}

void PCB::threadDispatchHandler()
{
    PCB::timeSliceCounter = 0;
    PCB::dispatch();
}

void PCB::threadSleepHandler()
{
    uint64 time;
    __asm__ volatile("mv %0, a1" : "=r"(time));
    PCB::timeSliceCounter = 0;
    PCB::running->setTimeToSleep(time + Riscv::totalTime);
    SleepPCBList::insertSleepingPCB();
    PCB::dispatch();
    __asm__ volatile("li a0, 0x0");
    Riscv::w_a0_sscratch();
}

void PCB::threadCreateHandler()
{
    uint64 start_routine;
    uint64 args;
    PCB **threadHandle;
    __asm__ volatile("mv %0, a1" : "=r"(threadHandle));
    __asm__ volatile("mv %0, a2" : "=r"(start_routine));
    __asm__ volatile("mv %0, a3" : "=r"(args));

    PCB *newPCB = new PCB((void (*)(void *)) start_routine, (void *) args,
                          //stack_space,
                          (void *) PCB::savedRegA4,
                          DEFAULT_TIME_SLICE);

    (*threadHandle) = newPCB;

    if (newPCB == nullptr) {
        __asm__ volatile("li a0, 0x01");
    }
    else
    {
        newPCB->start();
        __asm__ volatile("li a0, 0");
    }
    Riscv::w_a0_sscratch();
}

void PCB::threadStartHandler()
{
    PCB* pcb;
    __asm__ volatile("mv %0, a1" : "=r"(pcb));
    if(pcb != nullptr)
    {
        pcb->start();
        __asm__ volatile("li a0, 0");
    }
    else
    {
        __asm__ volatile("li a0, 0x01");
    }
    Riscv::w_a0_sscratch();
}

void PCB::threadMakePCBHandler()
{
    uint64 start_routine;
    uint64 args;
    PCB **threadHandle;
    __asm__ volatile("mv %0, a1" : "=r"(threadHandle));
    __asm__ volatile("mv %0, a2" : "=r"(start_routine));
    __asm__ volatile("mv %0, a3" : "=r"(args));

    PCB *newPCB = new PCB((void (*)(void *)) start_routine, (void *) args,
                          (void *) PCB::savedRegA4,
                          DEFAULT_TIME_SLICE);

    (*threadHandle) = newPCB;

    if (newPCB == nullptr)
    {
        __asm__ volatile("li a0, 0x01");
    }
    else
        __asm__ volatile("li a0, 0");
    Riscv::w_a0_sscratch();
}

void PCB::threadDelPCBHandler()
{
    PCB* pcb;
    __asm__ volatile("mv %0, a1" : "=r"(pcb));
    if(pcb != nullptr)
    {
        delete pcb;
        __asm__ volatile("li a0, 0x0");
    }
    else
    {
        __asm__ volatile("li a0, 0x1");
    }
    Riscv::w_a0_sscratch();
}
