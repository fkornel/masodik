#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>

// DXGI a GPU VRAM lekerdezesehez
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

#include "masodik/tui/resource_monitor.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace masodik::tui {

ResourceMonitor::ResourceMonitor() = default;
ResourceMonitor::~ResourceMonitor() = default;

ResourceSnapshot ResourceMonitor::snapshot() {
    ResourceSnapshot snap;

    // --- CPU terheles szamitasa ---
    // A GetSystemTimes-bol szamoljuk a kulonbseget az elozo mereshez kepest.
    FILETIME idle_time{}, kernel_time{}, user_time{};
    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        const auto to_u64 = [](const FILETIME& ft) -> std::uint64_t {
            return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        const auto idle = to_u64(idle_time);
        const auto kernel = to_u64(kernel_time);
        const auto user = to_u64(user_time);

        if (!first_cpu_sample_) {
            const auto idle_delta = idle - prev_idle_;
            const auto total_delta = (kernel - prev_kernel_) + (user - prev_user_);
            if (total_delta > 0) {
                snap.cpu_percent = (1.0 - static_cast<double>(idle_delta) / static_cast<double>(total_delta)) * 100.0;
                snap.cpu_percent = (std::max)(0.0, (std::min)(100.0, snap.cpu_percent));
            }
        }
        prev_idle_ = idle;
        prev_kernel_ = kernel;
        prev_user_ = user;
        first_cpu_sample_ = false;
    }

    // --- RAM hasznalat ---
    MEMORYSTATUSEX mem_status{};
    mem_status.dwLength = sizeof(mem_status);
    if (GlobalMemoryStatusEx(&mem_status)) {
        snap.ram_total_bytes = mem_status.ullTotalPhys;
        snap.ram_used_bytes = mem_status.ullTotalPhys - mem_status.ullAvailPhys;
        if (snap.ram_total_bytes > 0) {
            snap.ram_percent = static_cast<double>(snap.ram_used_bytes) * 100.0 /
                               static_cast<double>(snap.ram_total_bytes);
        }
    }

    // --- GPU VRAM (DXGI adapter lekerdezés) ---
    // Az elso adapter a dGPU (ha van), a masodik az iGPU.
    IDXGIFactory4* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory)))) {
        for (UINT adapter_index = 0; ; ++adapter_index) {
            IDXGIAdapter3* adapter = nullptr;
            if (factory->EnumAdapters1(adapter_index, reinterpret_cast<IDXGIAdapter1**>(&adapter)) == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            // Az IDXGIAdapter3 interfesz kell a VRAM lekerdezesehez
            IDXGIAdapter3* adapter3 = nullptr;
            if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3)))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO local_info{};
                DXGI_QUERY_VIDEO_MEMORY_INFO shared_info{};

                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_info))) {
                    DXGI_ADAPTER_DESC1 desc{};
                    adapter->GetDesc1(&desc);

                    const auto dedicated_total = desc.DedicatedVideoMemory;
                    const auto dedicated_used = local_info.CurrentUsage;

                    if (dedicated_total > 512 * 1024 * 1024ULL) {
                        // dGPU: >512MB dedikalt VRAM
                        if (snap.dgpu_vram_total_bytes == 0) {
                            snap.dgpu_vram_total_bytes = dedicated_total;
                            snap.dgpu_vram_used_bytes = dedicated_used;
                            if (dedicated_total > 0) {
                                snap.dgpu_vram_percent = static_cast<double>(dedicated_used) * 100.0 /
                                                         static_cast<double>(dedicated_total);
                            }
                        }
                    } else {
                        // iGPU: kicsi vagy nincs dedikalt VRAM → shared memory
                        if (snap.igpu_vram_total_bytes == 0) {
                            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &shared_info))) {
                                snap.igpu_vram_total_bytes = desc.SharedSystemMemory;
                                snap.igpu_vram_used_bytes = shared_info.CurrentUsage;
                                if (snap.igpu_vram_total_bytes > 0) {
                                    snap.igpu_vram_percent = static_cast<double>(snap.igpu_vram_used_bytes) * 100.0 /
                                                             static_cast<double>(snap.igpu_vram_total_bytes);
                                }
                            }
                        }
                    }
                }
                adapter3->Release();
            }
            adapter->Release();
        }
        factory->Release();
    }

    return snap;
}

}  // namespace masodik::tui
