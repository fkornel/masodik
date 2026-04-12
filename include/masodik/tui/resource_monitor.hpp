#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace masodik::tui {

// Per-core CPU terheles.
struct CoreUsage {
    double percent = 0.0;
    bool is_p_core = false;
};

// Rendszer eroforras pillanatkep — CPU (per-core), RAM, GPU VRAM adatok.
// A GPU VRAM ertekek a WMI Performance Counters-bol jonnek,
// amely pontosan megegyezik a Task Manager kijelzesevel.
struct ResourceSnapshot {
    double cpu_percent = 0.0;
    std::vector<CoreUsage> cores;  // 20 mag: 8P + 12E
    double cpu_frequency_ghz = 0.0;  // Aktualis orajel GHz-ben

    std::uint64_t ram_used_bytes = 0;
    std::uint64_t ram_total_bytes = 0;
    double ram_percent = 0.0;

    std::uint64_t dgpu_vram_used_bytes = 0;
    std::uint64_t dgpu_vram_total_bytes = 0;
    double dgpu_vram_percent = 0.0;

    std::uint64_t igpu_vram_used_bytes = 0;
    std::uint64_t igpu_vram_total_bytes = 0;
    double igpu_vram_percent = 0.0;
};

// Eroforras monitor — WMI alapu GPU memoria lekerdezes,
// GetSystemTimes CPU, GlobalMemoryStatusEx RAM.
class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    ResourceSnapshot snapshot();

private:
    // CPU globalis
    std::uint64_t prev_idle_ = 0;
    std::uint64_t prev_kernel_ = 0;
    std::uint64_t prev_user_ = 0;
    bool first_cpu_sample_ = true;
    // Per-core
    int num_cores_ = 0;
    int p_core_count_ = 8;
    std::vector<std::uint64_t> prev_core_idle_;
    std::vector<std::uint64_t> prev_core_kernel_;
    std::vector<std::uint64_t> prev_core_user_;

    // GPU adapter cache
    int dgpu_adapter_index_ = -1;
    int igpu_adapter_index_ = -1;
    std::uint64_t dgpu_dedicated_total_ = 0;
    std::uint64_t igpu_shared_total_ = 0;

    // WMI kapcsolat (impl rejtett a .cpp-ben)
    struct WmiState;
    std::unique_ptr<WmiState> wmi_;
};

}  // namespace masodik::tui
