#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Psapi.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

#include "masodik/tui/resource_monitor.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace masodik::tui {

namespace {

// WMI lekerdezeshez segitosztalyok — az IWbemServices kapcsolatot
// egyszer hozzuk letre a konstruktorban, utana minden snapshot()-ban
// ujrahasznaljuk. Ez gyorsabb mint minden alkalommal uj kapcsolat.
class WmiConnection {
public:
    WmiConnection() {
        // COM inicializacio
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        com_initialized_ = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

        if (!com_initialized_) return;

        // WMI locator
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IWbemLocator, reinterpret_cast<void**>(&locator_));
        if (FAILED(hr) || !locator_) return;

        // Kapcsolodas a root\cimv2 namespace-hez
        hr = locator_->ConnectServer(
            _bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services_);
        if (FAILED(hr) || !services_) return;

        // Proxy biztonsag beallitasa
        CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        connected_ = true;
    }

    ~WmiConnection() {
        if (services_) services_->Release();
        if (locator_) locator_->Release();
        if (com_initialized_) CoUninitialize();
    }

    // GPU adapter memoria lekerdezes — az osszes process osszesitett GPU
    // memoria hasznalatat adja vissza adapterenkent. Ez megegyezik a
    // Task Manager "Dedicated GPU memory" ertekevel.
    struct GpuMemoryInfo {
        std::uint64_t dedicated_usage = 0;  // Dedikalt VRAM hasznalat (osszes process)
        std::uint64_t shared_usage = 0;     // Megosztott memoria hasznalat
    };

    // Adapter LUID alapjan kerdezi le a memoria hasznalatot
    GpuMemoryInfo query_gpu_memory(const std::wstring& adapter_luid_filter) {
        GpuMemoryInfo result;
        if (!connected_ || !services_) return result;

        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = services_->ExecQuery(
            _bstr_t(L"WQL"),
            _bstr_t(L"SELECT * FROM Win32_PerfFormattedData_GPUPerformanceCounters_GPUAdapterMemory"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);

        if (FAILED(hr) || !enumerator) return result;

        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &returned) == S_OK && returned > 0) {
            VARIANT name_var{};
            if (SUCCEEDED(obj->Get(L"Name", 0, &name_var, nullptr, nullptr)) && name_var.vt == VT_BSTR) {
                std::wstring name(name_var.bstrVal);
                // Az adapter neve tartalmazza a LUID-ot — szurjuk
                if (!adapter_luid_filter.empty() && name.find(adapter_luid_filter) == std::wstring::npos) {
                    VariantClear(&name_var);
                    obj->Release();
                    continue;
                }
            }
            VariantClear(&name_var);

            // DedicatedUsage — a dedikalt VRAM hasznalat byte-ban
            VARIANT ded_var{};
            if (SUCCEEDED(obj->Get(L"DedicatedUsage", 0, &ded_var, nullptr, nullptr))) {
                if (ded_var.vt == VT_BSTR) {
                    result.dedicated_usage = _wcstoui64(ded_var.bstrVal, nullptr, 10);
                } else if (ded_var.vt == VT_I8 || ded_var.vt == VT_UI8) {
                    result.dedicated_usage = static_cast<std::uint64_t>(ded_var.ullVal);
                }
            }
            VariantClear(&ded_var);

            // SharedUsage
            VARIANT shared_var{};
            if (SUCCEEDED(obj->Get(L"SharedUsage", 0, &shared_var, nullptr, nullptr))) {
                if (shared_var.vt == VT_BSTR) {
                    result.shared_usage = _wcstoui64(shared_var.bstrVal, nullptr, 10);
                } else if (shared_var.vt == VT_I8 || shared_var.vt == VT_UI8) {
                    result.shared_usage = static_cast<std::uint64_t>(shared_var.ullVal);
                }
            }
            VariantClear(&shared_var);

            obj->Release();
            // Az elso talalat elég ha nincs LUID filter
            if (adapter_luid_filter.empty()) break;
        }
        enumerator->Release();
        return result;
    }

    // Nyers WQL lekerdezes — a hivo felelos az enumerator Release()-eert
    HRESULT query_raw(const wchar_t* wql, IEnumWbemClassObject** result) {
        if (!connected_ || !services_ || !result) return E_FAIL;
        return services_->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(wql),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, result);
    }

    [[nodiscard]] bool connected() const { return connected_; }

private:
    IWbemLocator* locator_ = nullptr;
    IWbemServices* services_ = nullptr;
    bool com_initialized_ = false;
    bool connected_ = false;
};

}  // namespace

struct ResourceMonitor::WmiState {
    WmiConnection connection;
    std::wstring dgpu_luid_filter;
    std::wstring igpu_luid_filter;
};

ResourceMonitor::ResourceMonitor() : wmi_(std::make_unique<WmiState>()) {
    // Per-core inicializalas
    SYSTEM_INFO sys_info{};
    GetSystemInfo(&sys_info);
    num_cores_ = static_cast<int>(sys_info.dwNumberOfProcessors);
    p_core_count_ = (std::min)(8, num_cores_);  // Core Ultra 265K: 8 P-core
    prev_core_idle_.resize(num_cores_, 0);
    prev_core_kernel_.resize(num_cores_, 0);
    prev_core_user_.resize(num_cores_, 0);

    // DXGI adapter azonositas — LUID-ok és total meretek cache-elese
    IDXGIFactory4* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory)))) {
        for (UINT i = 0; ; ++i) {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) break;

            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            // A LUID-ot hex stringge alakitjuk a WMI szureshez
            // A WMI Name mezoben a LUID "luid_0x00000000_0xNNNNNNNN" formatumban van
            wchar_t luid_str[64];
            swprintf_s(luid_str, L"luid_0x%08X_0x%08X",
                       static_cast<unsigned>(desc.AdapterLuid.HighPart),
                       static_cast<unsigned>(desc.AdapterLuid.LowPart));

            if (desc.DedicatedVideoMemory > 512ULL * 1024 * 1024 && dgpu_adapter_index_ < 0) {
                dgpu_adapter_index_ = static_cast<int>(i);
                dgpu_dedicated_total_ = desc.DedicatedVideoMemory;
                wmi_->dgpu_luid_filter = luid_str;
            } else if (desc.VendorId == 0x8086 && dgpu_adapter_index_ != static_cast<int>(i) && igpu_adapter_index_ < 0) {
                igpu_adapter_index_ = static_cast<int>(i);
                igpu_shared_total_ = desc.SharedSystemMemory;
                wmi_->igpu_luid_filter = luid_str;
            }
            adapter->Release();
        }
        factory->Release();
    }
}

ResourceMonitor::~ResourceMonitor() = default;

ResourceSnapshot ResourceMonitor::snapshot() {
    ResourceSnapshot snap;

    // --- CPU (globalis + per-core + frekvencia) ---
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
    }

    // Per-core: NtQuerySystemInformation (SystemProcessorPerformanceInformation = 8)
    // A globalis GetSystemTimes-szal kalibraljuk, hogy az atlag megegyezzen.
    struct SPPI { LARGE_INTEGER IdleTime; LARGE_INTEGER KernelTime; LARGE_INTEGER UserTime; };
    std::vector<SPPI> sppi(num_cores_);
    using NtQSI = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);
    static const auto ntdll = GetModuleHandleA("ntdll.dll");
    static const auto pNtQSI = ntdll ? reinterpret_cast<NtQSI>(GetProcAddress(ntdll, "NtQuerySystemInformation")) : nullptr;

    if (pNtQSI && !first_cpu_sample_) {
        ULONG returned = 0;
        if (pNtQSI(8, sppi.data(), static_cast<ULONG>(num_cores_ * sizeof(SPPI)), &returned) == 0) {
            const int n = (std::min)(static_cast<int>(returned / sizeof(SPPI)), num_cores_);
            snap.cores.resize(n);
            double raw_sum = 0.0;
            std::vector<double> raw_pcts(n, 0.0);
            for (int c = 0; c < n; ++c) {
                const auto ci = static_cast<std::uint64_t>(sppi[c].IdleTime.QuadPart);
                const auto ck = static_cast<std::uint64_t>(sppi[c].KernelTime.QuadPart);
                const auto cu = static_cast<std::uint64_t>(sppi[c].UserTime.QuadPart);
                const auto idle_d = ci - prev_core_idle_[c];
                const auto total_d = (ck - prev_core_kernel_[c]) + (cu - prev_core_user_[c]);
                if (total_d > 0) {
                    raw_pcts[c] = (1.0 - static_cast<double>(idle_d) / static_cast<double>(total_d)) * 100.0;
                    raw_pcts[c] = (std::max)(0.0, (std::min)(100.0, raw_pcts[c]));
                }
                raw_sum += raw_pcts[c];
                prev_core_idle_[c] = ci;
                prev_core_kernel_[c] = ck;
                prev_core_user_[c] = cu;
            }
            // Kalibralas a globalis CPU%-hoz
            const double raw_avg = n > 0 ? raw_sum / n : 0.0;
            const double scale = (raw_avg > 0.1 && snap.cpu_percent > 0.0) ? snap.cpu_percent / raw_avg : 1.0;
            for (int c = 0; c < n; ++c) {
                snap.cores[c].percent = (std::max)(0.0, (std::min)(100.0, raw_pcts[c] * scale));
                snap.cores[c].is_p_core = (c < p_core_count_);
            }
        }
    } else if (pNtQSI && first_cpu_sample_) {
        ULONG returned = 0;
        if (pNtQSI(8, sppi.data(), static_cast<ULONG>(num_cores_ * sizeof(SPPI)), &returned) == 0) {
            const int n = (std::min)(static_cast<int>(returned / sizeof(SPPI)), num_cores_);
            for (int c = 0; c < n; ++c) {
                prev_core_idle_[c] = static_cast<std::uint64_t>(sppi[c].IdleTime.QuadPart);
                prev_core_kernel_[c] = static_cast<std::uint64_t>(sppi[c].KernelTime.QuadPart);
                prev_core_user_[c] = static_cast<std::uint64_t>(sppi[c].UserTime.QuadPart);
            }
        }
    }
    first_cpu_sample_ = false;

    // CPU frekvencia — WMI ProcessorInformation PercentofMaximumFrequency
    // A ProcessorFrequency a base clock (pl. 3540 MHz), a PercentofMaximumFrequency
    // a pillanatnyi skalazas (pl. 41%). A tenyleges orajel: base * percent / 100.
    // Ez a Task Manager-rel megegyezo dinamikus ertek.
    if (wmi_ && wmi_->connection.connected()) {
        IEnumWbemClassObject* freq_enum = nullptr;
        HRESULT hr = wmi_->connection.query_raw(
            L"SELECT ProcessorFrequency, PercentProcessorPerformance "
            L"FROM Win32_PerfFormattedData_Counters_ProcessorInformation "
            L"WHERE Name='_Total'",
            &freq_enum);
        if (SUCCEEDED(hr) && freq_enum) {
            IWbemClassObject* obj = nullptr;
            ULONG ret = 0;
            if (freq_enum->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK && ret > 0) {
                VARIANT freq_var{}, pct_var{};
                ULONG base_mhz = 0, pct_max = 0;
                if (SUCCEEDED(obj->Get(L"ProcessorFrequency", 0, &freq_var, nullptr, nullptr))) {
                    if (freq_var.vt == VT_BSTR) base_mhz = static_cast<ULONG>(_wtol(freq_var.bstrVal));
                    else if (freq_var.vt == VT_I4 || freq_var.vt == VT_UI4) base_mhz = freq_var.ulVal;
                }
                VariantClear(&freq_var);
                if (SUCCEEDED(obj->Get(L"PercentProcessorPerformance", 0, &pct_var, nullptr, nullptr))) {
                    if (pct_var.vt == VT_BSTR) pct_max = static_cast<ULONG>(_wtol(pct_var.bstrVal));
                    else if (pct_var.vt == VT_I4 || pct_var.vt == VT_UI4) pct_max = pct_var.ulVal;
                }
                VariantClear(&pct_var);

                if (base_mhz > 0 && pct_max > 0) {
                    snap.cpu_frequency_ghz = static_cast<double>(base_mhz) * pct_max / 100.0 / 1000.0;
                }
                obj->Release();
            }
            freq_enum->Release();
        }
    }

    // --- RAM ---
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

    // --- GPU VRAM (WMI Performance Counters) ---
    // A Win32_PerfFormattedData_GPUPerformanceCounters_GPUAdapterMemory
    // osztaly az osszes process osszesitett GPU memoria hasznalatot adja.
    // Ez pontosan megegyezik a Task Manager "Dedicated GPU memory" ertekevel.
    if (wmi_ && wmi_->connection.connected()) {
        // dGPU
        if (dgpu_adapter_index_ >= 0) {
            const auto mem = wmi_->connection.query_gpu_memory(wmi_->dgpu_luid_filter);
            snap.dgpu_vram_total_bytes = dgpu_dedicated_total_;
            snap.dgpu_vram_used_bytes = mem.dedicated_usage;
            if (snap.dgpu_vram_total_bytes > 0) {
                snap.dgpu_vram_percent = static_cast<double>(snap.dgpu_vram_used_bytes) * 100.0 /
                                         static_cast<double>(snap.dgpu_vram_total_bytes);
                snap.dgpu_vram_percent = (std::max)(0.0, (std::min)(100.0, snap.dgpu_vram_percent));
            }
        }

        // iGPU
        if (igpu_adapter_index_ >= 0) {
            const auto mem = wmi_->connection.query_gpu_memory(wmi_->igpu_luid_filter);
            snap.igpu_vram_total_bytes = igpu_shared_total_;
            snap.igpu_vram_used_bytes = mem.shared_usage;
            if (snap.igpu_vram_total_bytes > 0) {
                snap.igpu_vram_percent = static_cast<double>(snap.igpu_vram_used_bytes) * 100.0 /
                                         static_cast<double>(snap.igpu_vram_total_bytes);
                snap.igpu_vram_percent = (std::max)(0.0, (std::min)(100.0, snap.igpu_vram_percent));
            }
        }
    }

    return snap;
}

}  // namespace masodik::tui
