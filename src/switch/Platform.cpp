/*
    Copyright 2018 Hydr8gon

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <malloc.h>
#include <stdio.h>
#include <switch.h>

// Deal with conflicting typedefs
#define u64 u64_
#define s64 s64_

#include "../Platform.h"
#include "../Config.h"

namespace Platform
{

void StopEmu()
{
}

void* Thread_Create(void (*func)())
{
    Thread* thread = (Thread*)malloc(sizeof(Thread));
    threadCreate(thread, (void(*)(void*))func, NULL, 0x80000, 0x30, 2);
    threadStart(thread);
    return thread;
}

void Thread_Free(void* thread)
{
    delete (Thread*)thread;
}

void Thread_Wait(void* thread)
{
    threadWaitForExit((Thread*)thread);
}

void* Semaphore_Create()
{
    Semaphore* sema = (Semaphore*)malloc(sizeof(Semaphore));
    semaphoreInit(sema, 0);
    return sema;
}

void Semaphore_Free(void* sema)
{
    delete (Semaphore*)sema;
}

void Semaphore_Reset(void* sema)
{
    while (semaphoreTryWait((Semaphore*)sema) == true);
}

void Semaphore_Wait(void* sema)
{
    semaphoreWait((Semaphore*)sema);
}

void Semaphore_Post(void* sema)
{
    semaphoreSignal((Semaphore*)sema);
}

bool MP_Init()
{
    return false;
}

void MP_DeInit()
{
}

int MP_SendPacket(u8* data, int len)
{
    return 0;
}

int MP_RecvPacket(u8* data, bool block)
{
    return 0;
}

bool TryLoadPCap(void* lib)
{
    return true;
}

bool LAN_Init()
{
    return false;
}

void LAN_DeInit()
{
}

int LAN_SendPacket(u8* data, int len)
{
    return 0;
}

void LAN_RXCallback(u_char* blarg, const struct pcap_pkthdr* header, const u_char* data)
{
}

int LAN_RecvPacket(u8* data)
{
    return 0;
}

}
