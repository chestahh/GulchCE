#include "harness.h"
#include <stddef.h>

#define WINAPI
typedef void *HANDLE, *HMODULE;
typedef unsigned long DWORD_PTR, ULONG, *PULONG;
typedef int BOOL;
typedef struct {
    ULONG Size, Type;
    struct { unsigned int Group, LogicalProcessorIndex, EfficiencyClass, Allocated, AllocatedToTargetProcess; } CpuSet;
} SYSTEM_CPU_SET_INFORMATION, *PSYSTEM_CPU_SET_INFORMATION;
enum { CpuSetInformation };
static SYSTEM_CPU_SET_INFORMATION records[4];
static ULONG assigned, groups = 1, processors = 4;
static DWORD_PTR allowed = 15, applied;
static int available = 1, writes;

static HANDLE GetCurrentProcess(void) { return (HANDLE)1; }
static HMODULE GetModuleHandleW(const void *name) { return (HMODULE)1; }
static ULONG GetActiveProcessorGroupCount(void) { return groups; }
static ULONG GetActiveProcessorCount(int group) { return processors; }
static BOOL GetProcessAffinityMask(HANDLE p, DWORD_PTR *a, DWORD_PTR *s) { *a = allowed; *s = 15; return TRUE; }
static BOOL SetProcessAffinityMask(HANDLE p, DWORD_PTR mask) { writes++; applied = mask; return TRUE; }
static BOOL cpu_sets(PSYSTEM_CPU_SET_INFORMATION out, ULONG size, PULONG used, HANDLE p, ULONG flags)
{
    *used = sizeof(records);
    if (size < *used) return FALSE;
    memcpy(out, records, *used);
    return TRUE;
}
static BOOL defaults(HANDLE p, PULONG ids, ULONG count, PULONG used) { *used = assigned; return TRUE; }
static void (*GetProcAddress(HMODULE module, const char *name))(void)
{
    if (!available) return NULL;
    return !strcmp(name,"GetProcessDefaultCpuSets") ? (void (*)(void))defaults : (void (*)(void))cpu_sets;
}
static void platform_log(const char *format, ...) {}
#include "policy.inc"

int main(void)
{
    int i;
    for (i=0; i<4; i++) {
        records[i].Size = sizeof(records[i]);
        records[i].CpuSet.LogicalProcessorIndex = i;
        records[i].CpuSet.EfficiencyClass = (i >= 2);
    }
    win32_cpu_scheduling_start(FALSE);
    CHECK(win32_cpu_scheduling_supported() && !writes, "Auto preserves inherited affinity");
    win32_cpu_scheduling_start(TRUE);
    CHECK(writes==1 && applied==12, "selects both available performance processors");
    allowed=6; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==2 && applied==4, "respects launcher affinity");
    records[2].CpuSet.Allocated=1; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==2 && !win32_cpu_scheduling_supported(), "respects a reserved processor");
    records[2].CpuSet.AllocatedToTargetProcess=1; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==3 && applied==4, "permits reservation belonging to this process");
    allowed=3; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==3, "E-core-only inherited mask cannot expand");
    allowed=15; assigned=1; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==3, "respects existing CPU-set policy");
    assigned=0; groups=2; win32_cpu_scheduling_start(TRUE);
    groups=1; processors=64; win32_cpu_scheduling_start(TRUE);
    processors=4; available=0; win32_cpu_scheduling_start(TRUE);
    CHECK(writes==3, "unsupported group, mask width and API preserve Auto");
    available=1; records[2].CpuSet.EfficiencyClass=records[3].CpuSet.EfficiencyClass=0;
    win32_cpu_scheduling_start(TRUE); CHECK(writes==3, "uniform class preserves Auto");
    records[2].CpuSet.EfficiencyClass=1; records[3].Size=0;
    win32_cpu_scheduling_start(TRUE);
    CHECK(writes==3, "incomplete topology cannot authorize a partial selection");
    return 0;
}
