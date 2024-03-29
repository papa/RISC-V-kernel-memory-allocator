//
// Created by os on 5/19/22.
//
#include "../../h/KConsole.hpp"
#include "../user/test/printing.hpp"
#include "../../h/Riscv.hpp"
#include "../../h/syscall_c_kernel.hpp"

uint64 KConsole::inputHead = 0;
uint64 KConsole::inputTail = 0;
uint64 KConsole::outputHead = 0;
uint64 KConsole::outputTail = 0;
KSemaphore* KConsole::hasCharactersOutput = nullptr;
KSemaphore* KConsole::hasCharactersInput = nullptr;
uint64 KConsole::pendingGetc = 0;
//char KConsole::inputBuffer[bufferSize];
//char KConsole::outputBuffer[bufferSize];
char* KConsole::inputBuffer = nullptr;
char* KConsole::outputBuffer = nullptr;
uint64 KConsole::pendingPutc = 0;
bool KConsole::finished = false;

void KConsole::initialize()
{
    hasCharactersInput = new KSemaphore(0);
    hasCharactersOutput = new KSemaphore(0);
    inputBuffer = (char*)kmalloc(bufferSize);
    outputBuffer = (char*)kmalloc(bufferSize);
}

//extern const uint64 CONSOLE_STATUS;
//extern const uint64 CONSOLE_TX_DATA;
//extern const uint64 CONSOLE_RX_DATA;
void KConsole::getCharactersFromConsole()
{
    uint64 operation = *(uint8*)CONSOLE_STATUS;
    if(operation & KConsole::STATUS_READ_MASK)
    {
        char c = *(uint8*)CONSOLE_TX_DATA;
        if(KConsole::pendingGetc > 0)
        {
            KConsole::pendingGetc--;
            KConsole::putCharacterInput(c);
            //KConsole::putCharacterOutput(c);
        }
    }
}

void KConsole::sendCharactersToConsole(void* p)
{
    while(true)
    {
            if(Riscv::finishSystem && KConsole::outputBufferEmpty() && pendingGetc == 0)
            {
                thread_exit_kernel();
            }

            uint64 operation = *(uint8*)CONSOLE_STATUS;
            if ((operation & STATUS_WRITE_MASK) && pendingPutc > 0)
            {
                char volatile c = sysCallGetCharOutput();
                pendingPutc--;
                *(uint8*)CONSOLE_RX_DATA = c;
            }
            else
            {
                thread_dispatch_kernel();
            }
    }
}

void KConsole::putCharacterInput(char c)
{
    if((inputTail+1)%bufferSize == inputHead)
        return;
    inputBuffer[inputTail] = c;
    inputTail = (inputTail+1)%bufferSize;
    hasCharactersInput->signal();
}

char KConsole::getCharacterInput()
{
    hasCharactersInput->wait();
    if(inputHead == inputTail)
        return -1;
    char c = inputBuffer[inputHead];
    inputHead = (inputHead+1)%bufferSize;
    return c;
}

void KConsole::putCharacterOutput(char c)
{
    if((outputTail+1)%bufferSize == outputHead)
        return;
    pendingPutc++;
    outputBuffer[outputTail] = c;
    outputTail = (outputTail+1)%bufferSize;
    hasCharactersOutput->signal();
}

char KConsole::getCharacterOutput()
{
    hasCharactersOutput->wait();
    if(outputHead == outputTail)
        return -1;
    char c = outputBuffer[outputHead];
    outputHead = (outputHead+1)%bufferSize;
    return c;
}

void KConsole::putcHandler()
{
    uint64 ch;
    __asm__ volatile("mv %0, a1" : "=r"(ch));
    putCharacterOutput((char)ch);
}

void KConsole::getcHandler()
{
    pendingGetc++;
    char ch;
    ch = getCharacterInput();
    if(ch!=0x01b)
        putCharacterOutput(ch);

    if(ch==13)
    {
        putCharacterOutput(13);
        putCharacterOutput(10);
    }

    __asm__ volatile("mv a0, %0" : :"r"((uint64)ch));
    Riscv::w_a0_sscratch();
}

bool KConsole::outputBufferEmpty()
{
    return pendingPutc == 0;
}

void KConsole::getCharHandler()
{
    char ch;
    ch = getCharacterOutput();
    __asm__ volatile("mv a0, %0" : :"r"((uint64)ch));
    Riscv::w_a0_sscratch();
}

void KConsole::trapPrintString(const char *string)
{
    while (*string != '\0')
    {
        KConsole::putCharacterOutput(*string);
        string++;
    }
}

void KConsole::trapPrintInt(size_t xx, int base, int sgn)
{
    char digits[] = "0123456789ABCDEF";
    char buf[16];
    int i, neg;
    size_t x;

    neg = 0;
    if(sgn && xx < 0) {
        neg = 1;
        x = -xx;
    } else {
        x = xx;
    }

    i = 0;
    do {
        buf[i++] = digits[x % base];
    }while((x /= base) != 0);
    if(neg)
        buf[i++] = '-';

    while(--i >= 0)
        KConsole::putCharacterOutput(buf[i]);
}

void KConsole::trapPrintStringInt(const char *string, size_t xx, int base)
{
    trapPrintString(string);
    trapPrintInt(xx,base);
    trapPrintString("\n");
}
