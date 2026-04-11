#pragma once

#include <cstdint>
#include <string>

namespace masodik::tui {

// Rendszer eroforras pillanatkep — CPU, RAM, GPU VRAM adatok.
// Windows API-n keresztul kerdezi le a valos ideju ertekeket.
struct ResourceSnapshot {
    // CPU terheles szazalekban (0-100)
    double cpu_percent = 0.0;

    // RAM (fizikai memoria)
    std::uint64_t ram_used_bytes = 0;
    std::uint64_t ram_total_bytes = 0;
    double ram_percent = 0.0;

    // dGPU VRAM (dedikalt GPU memoria)
    std::uint64_t dgpu_vram_used_bytes = 0;
    std::uint64_t dgpu_vram_total_bytes = 0;
    double dgpu_vram_percent = 0.0;

    // iGPU VRAM (megosztott GPU memoria)
    std::uint64_t igpu_vram_used_bytes = 0;
    std::uint64_t igpu_vram_total_bytes = 0;
    double igpu_vram_percent = 0.0;
};

// Eroforras monitor — periodikusan lekerdezheto a rendszer allapota.
class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    // Aktualis pillanatkep lekerdezese (Windows API hivasok).
    ResourceSnapshot snapshot();

private:
    // CPU szamitashoz az elozo meresi pont
    std::uint64_t prev_idle_ = 0;
    std::uint64_t prev_kernel_ = 0;
    std::uint64_t prev_user_ = 0;
    bool first_cpu_sample_ = true;
};

}  // namespace masodik::tui
