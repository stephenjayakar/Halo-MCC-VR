// Opt-in observation only. Official/retail call evidence and limitations:
// docs/HALO3-CLEARANCE-QUERY-EVIDENCE.md. No result controls weapon rendering.
using Halo3ClearancePointFn = bool(__fastcall*)(uint64_t, const float*, int32_t, int32_t, int32_t*);
using Halo3ClearanceGatherFn = bool(__fastcall*)(uint64_t, const float*, float, float,
                                               float, int32_t, int32_t, void*);
Halo3ClearancePointFn g_halo3ClearancePoint = nullptr;
Halo3ClearanceGatherFn g_halo3ClearanceGather = nullptr;
std::atomic<bool> g_halo3ClearanceProbeEnabled{false};
struct Halo3ClearanceProbeRecord
{
    uint64_t ms{}, flags{};
    PhysicalContactVec3 center{};
    float radius{};
    int32_t ignored{-1}, pointType{};
    uint16_t counts[3]{};
    bool pointHit{}, gathered{}, faulted{}, boundsValid{};
    double microseconds{};
};
std::array<Halo3ClearanceProbeRecord, 32> g_halo3ClearanceRecords{};
std::array<std::atomic<uint32_t>, 32> g_halo3ClearanceRecordStates{};

void Halo3BindClearanceProbe(uintptr_t base, size_t size)
{
    g_halo3ClearanceProbeEnabled.store(false, std::memory_order_release);
    wchar_t value[2]{};
    if (GetEnvironmentVariableW(L"HALOMCCVR_H3_CONTACT_DEBUG_CLEARANCE", value, 2) != 1 || value[0] != L'1')
        return;
    const auto unique = [&](const char* pattern) -> uintptr_t {
        const uintptr_t hit = sig::Find(base, size, pattern);
        return hit && !sig::Find(hit + 1, base + size - hit - 1, pattern) ? hit : 0;
    };
    const uintptr_t point = unique("48 8B C4 44 89 48 20 44 89 40 18 48 89 50 10 48 89 48 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 68 A9 48 81 EC B8 00 00 00");
    const uintptr_t gather = unique("48 8B C4 48 89 58 20 48 89 50 10 48 89 48 08 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 18 C7 FF FF B8 B0 39 00 00 E8 ?? ?? ?? ?? 48 2B E0");
    // The entire candidate is pinned; offsets are additional evidence checks,
    // never a fallback when either unique signature fails.
    if (!point || !gather || point - base != 0x1FCAC0 || gather - base != 0x1FE800)
    {
        LOG("H3 clearance PROBE: unique verified bindings unavailable; probe disabled; VR unchanged");
        return;
    }
    g_halo3ClearancePoint = reinterpret_cast<Halo3ClearancePointFn>(point);
    g_halo3ClearanceGather = reinterpret_cast<Halo3ClearanceGatherFn>(gather);
    g_halo3ClearanceProbeEnabled.store(true, std::memory_order_release);
    LOG("H3 clearance PROBE enabled: observation only, 32 bounded samples, no render approval");
}

void Halo3RunClearanceProbe(uint64_t nowMs, PhysicalContactVec3 center,
                            float worldScale, int32_t unitHandle)
{
    if (!g_halo3ClearanceProbeEnabled.load(std::memory_order_acquire) ||
        !g_camValid.load(std::memory_order_acquire) ||
        !PhysicalContactFinite(center) || !std::isfinite(worldScale) ||
        worldScale < 0.05f || worldScale > 2.0f)
        return;
    static uint32_t next = 0;
    static uint64_t lastMs = 0;
    if (next >= g_halo3ClearanceRecords.size() || nowMs < lastMs || nowMs - lastMs < 200)
        return;
    lastMs = nowMs;
    const uint32_t index = next++;
    auto& record = g_halo3ClearanceRecords[index];
    record.ms = nowMs;
    record.center = center;
    record.radius = (index % 4 + 1) * 0.10f * worldScale;
    record.flags = 9ull | ((index & 4u) ? (0x7FFFull << 32) : 0ull);
    record.ignored = unitHandle;
    // Oversized local storage with a checked tail. Only the verified three
    // count fields are interpreted; unused records are never read as geometry.
    alignas(16) unsigned char storage[0x10040];
    memset(storage, 0xCD, sizeof(storage));
    LARGE_INTEGER begin{}, end{}, frequency{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&begin);
    __try
    {
        record.pointHit = g_halo3ClearancePoint(record.flags, &center.x, unitHandle, -1, &record.pointType);
        record.gathered = g_halo3ClearanceGather(record.flags, &center.x, record.radius,
                                                0.0f, 0.0f, unitHandle, -1, storage);
        memcpy(record.counts, storage, sizeof(record.counts));
        record.boundsValid = record.counts[0] <= 256 && record.counts[1] <= 256 && record.counts[2] <= 256;
        for (size_t offset = 0xC408; offset < sizeof(storage); ++offset)
            record.boundsValid = record.boundsValid && storage[offset] == 0xCD;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        record.faulted = true;
    }
    QueryPerformanceCounter(&end);
    if (frequency.QuadPart > 0)
        record.microseconds = (end.QuadPart - begin.QuadPart) * 1.0e6 / frequency.QuadPart;
    if (record.faulted || !record.boundsValid)
        g_halo3ClearanceProbeEnabled.store(false, std::memory_order_release);
    g_halo3ClearanceRecordStates[index].store(2, std::memory_order_release);
}

void Halo3LogClearanceProbe()
{
    for (uint32_t index = 0; index < g_halo3ClearanceRecords.size(); ++index)
    {
        uint32_t expected = 2;
        if (!g_halo3ClearanceRecordStates[index].compare_exchange_strong(expected, 3, std::memory_order_acquire))
            continue;
        const auto& r = g_halo3ClearanceRecords[index];
        LOG("H3 clearance PROBE sample: index=%u ms=%llu flags=%llX center=(%.6f %.6f %.6f) radius=%.6f ignored=%08X pointHit=%d pointType=%d gathered=%d counts=%u/%u/%u bounds=%d faulted=%d costUs=%.1f",
            index, r.ms, r.flags, r.center.x, r.center.y, r.center.z, r.radius, r.ignored,
            r.pointHit, r.pointType, r.gathered, r.counts[0], r.counts[1], r.counts[2],
            r.boundsValid, r.faulted, r.microseconds);
    }
}
