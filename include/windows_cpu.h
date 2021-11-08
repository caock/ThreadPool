#pragma once

#include <windows.h>
#include <thread>

typedef DWORD cpu_set_t;

static inline void
CPU_ZERO(cpu_set_t* cs) { *cs = 0; }

static inline void
CPU_SET(int num, cpu_set_t* cs) { *cs |= (1 << num); }

static inline int
CPU_ISSET(int num, cpu_set_t* cs) { return (*cs & (1 << num)); }

int pthread_setaffinity_np(HANDLE pThread, size_t cpu_size, cpu_set_t* cpu_set)
{
  int core;
  for (core = 0; core < 8 * cpu_size; core++)
  {
    if (CPU_ISSET(core, cpu_set))
        break;
  }
  printf("binding to core %d\n", core);
  SetThreadAffinityMask(pThread, (DWORD_PTR)cpu_set);

  return 0;
}
