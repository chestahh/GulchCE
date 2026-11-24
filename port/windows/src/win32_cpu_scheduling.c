/* Optional process affinity for the local Windows client. */
#include <windows.h>
#include <stdlib.h>
#include <stddef.h>
#include "../../linux/src/win32_cpu_scheduling.h"

void platform_log(const char *format, ...);

static DWORD_PTR available_performance_cores(void)
{
	typedef BOOL (WINAPI *get_cpu_sets_fn)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);
	typedef BOOL (WINAPI *get_default_sets_fn)(HANDLE, PULONG, ULONG, PULONG);
	HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
	get_cpu_sets_fn get_sets = (get_cpu_sets_fn)GetProcAddress(kernel, "GetSystemCpuSetInformation");
	get_default_sets_fn get_defaults = (get_default_sets_fn)GetProcAddress(kernel, "GetProcessDefaultCpuSets");
	HANDLE process = GetCurrentProcess();
	DWORD_PTR allowed, system, fastest = 0;
	ULONG size = 0, used, offset, assigned;
	int highest = -1, lowest = 256;
	unsigned char *buffer;

	/* The 32-bit client cannot express affinities outside its mask. Also
	   defer to a launcher that already assigned CPU sets. */
	if (!get_sets || !get_defaults || GetActiveProcessorGroupCount() != 1 ||
		GetActiveProcessorCount(0) > sizeof(allowed) * 8 ||
		!GetProcessAffinityMask(process, &allowed, &system) ||
		!get_defaults(process, NULL, 0, &assigned) || assigned)
		return 0;
	get_sets(NULL, 0, &size, process, 0);
	if (!size) return 0;
	buffer = malloc(size);
	if (!buffer) return 0;
	if (!get_sets((PSYSTEM_CPU_SET_INFORMATION)buffer, size, &used, process, 0) || used > size)
		used = 0;
	for (offset = 0; offset < used; )
	{
		PSYSTEM_CPU_SET_INFORMATION item = (PSYSTEM_CPU_SET_INFORMATION)(buffer + offset);
		int level;
		if (used - offset < offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet) ||
			item->Size < offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet) || item->Size > used - offset)
			break;
		offset += item->Size;
		if (item->Type != CpuSetInformation) continue;
		if (item->Size < sizeof(*item) || item->CpuSet.Group ||
			item->CpuSet.LogicalProcessorIndex >= sizeof(allowed) * 8)
		{
			fastest = 0;
			break;
		}
		level = item->CpuSet.EfficiencyClass;
		if (level < lowest) lowest = level;
		if (level > highest) { highest = level; fastest = 0; }
		if (level == highest && (!item->CpuSet.Allocated || item->CpuSet.AllocatedToTargetProcess))
			fastest |= (DWORD_PTR)1 << item->CpuSet.LogicalProcessorIndex;
	}
	free(buffer);
	return offset == used && highest > lowest ? fastest & allowed : 0;
}

static int supported;

void win32_cpu_scheduling_start(int performance_only)
{
	DWORD_PTR mask = available_performance_cores();
	supported = mask != 0;
	if (performance_only)
	{
		if (mask && SetProcessAffinityMask(GetCurrentProcess(), mask))
			platform_log("CPU scheduling: performance cores only (mask 0x%lx)", (unsigned long)mask);
		else
			platform_log("CPU scheduling: retaining existing affinity (performance cores unavailable)");
	}
}

int win32_cpu_scheduling_supported(void)
{
	return supported;
}
