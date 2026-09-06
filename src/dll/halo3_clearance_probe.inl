// Opt-in native observations and experimental empty-region tracking.
// Evidence and remaining coverage limitations: HALO3-CLEARANCE-QUERY-EVIDENCE.md.
using Halo3ClearancePointFn = bool(__fastcall*)(uint64_t, const float*, int32_t, int32_t, int32_t*);
using Halo3ClearanceGatherFn = bool(__fastcall*)(uint64_t, const float*, float, float,
                                               float, int32_t, int32_t, void*);
Halo3ClearancePointFn g_halo3ClearancePoint = nullptr;
Halo3ClearanceGatherFn g_halo3ClearanceGather = nullptr;
std::atomic<bool> g_halo3ClearanceProbeEnabled{false};
std::atomic<bool> g_halo3FreshRegionEnabled{false};
const uint32_t* g_halo3ClearanceActiveMask = nullptr;
std::atomic<uint64_t> g_halo3FreshRegionQueries{0}, g_halo3FreshRegionClears{0};
std::atomic<uint64_t> g_halo3FreshRegionFrames{0}, g_halo3FreshRegionFaults{0};
std::atomic<uint64_t> g_halo3FreshRegionShapeRejects{0};
struct Halo3FreshRegionPublication
{
    std::atomic<uint32_t> sequence{0}, generation{0};
    std::atomic<uint64_t> epoch{0}, serial{0}, sampleMs{0};
    std::atomic<float> x{0}, y{0}, z{0}, allowance{0};
} g_halo3FreshRegion;
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
    g_halo3FreshRegionEnabled.store(false, std::memory_order_release);
    g_halo3ClearanceActiveMask = nullptr;
    wchar_t value[2]{};
    const bool probeRequested = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_CLEARANCE", value, 2) == 1 && value[0] == L'1';
    constexpr bool kEnableHalo3FreshRegionExperiment = true;
    const bool freshRequested = kEnableHalo3FreshRegionExperiment && GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_FRESH_REGION", value, 2) == 1 && value[0] == L'1';
    if (!probeRequested && !freshRequested)
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
    g_halo3ClearanceProbeEnabled.store(probeRequested, std::memory_order_release);
    if (probeRequested)
        LOG("H3 clearance PROBE enabled: observation only, 32 bounded samples, no render approval");
    if (freshRequested)
    {
        // Both verified native queries read the same active-structure mask.
        // Resolve the RIP operands from the matched functions, never from a
        // guessed global address. No active structure means unknown, not clear.
        const auto* p = reinterpret_cast<const uint8_t*>(point + 0x60);
        const auto* g = reinterpret_cast<const uint8_t*>(gather + 0x99);
        int32_t pd = 0, gd = 0;
        memcpy(&pd, p + 2, sizeof(pd));
        memcpy(&gd, g + 2, sizeof(gd));
        const uintptr_t pm = point + 0x66 + pd;
        const uintptr_t gm = gather + 0x9F + gd;
        if (p[0] == 0x8B && p[1] == 0x3D && g[0] == 0x8B && g[1] == 0x05 &&
            pm == gm && pm >= base && pm + 4 <= base + size && pm - base == 0x46B70A8)
        {
            g_halo3ClearanceActiveMask = reinterpret_cast<const uint32_t*>(pm);
            g_halo3FreshRegionEnabled.store(true, std::memory_order_release);
        }
        LOG("H3 fresh region EXPERIMENT: %s; full shape bound + 0.30m motion + 0.25m reserve; 20ms/one object epoch; native-query coverage remains under test",
            g_halo3FreshRegionEnabled.load() ? "enabled" : "disabled: active structure binding unavailable");
    }
}

void Halo3PublishFreshRegion(uint64_t nowMs, uint32_t generation, uint64_t serial,
    PhysicalContactVec3 center, float bound, float worldScale, int32_t ignored,
    bool eligible)
{
    if (!g_halo3FreshRegionEnabled.load(std::memory_order_acquire))
        return;
    auto& out = g_halo3FreshRegion;
    out.sequence.fetch_add(1, std::memory_order_acq_rel);
    out.epoch.store(0, std::memory_order_relaxed);
    out.sequence.fetch_add(1, std::memory_order_release);
    if (!eligible || !g_halo3ContactLateUpdateInstalled.load(std::memory_order_acquire) ||
        !generation || !serial || !PhysicalContactFinite(center) ||
        !std::isfinite(worldScale) || worldScale < .05f || worldScale > 2.0f ||
        !std::isfinite(bound) || bound < .02f * worldScale || bound > 2.5f * worldScale)
        return;
    const uint64_t epoch = g_halo3ClearanceEpoch.load(std::memory_order_acquire);
    const float allowance = .30f * worldScale;
    const float radius = bound + allowance + .25f * worldScale;
    alignas(16) unsigned char storage[0xC490]{};
    bool clear = false, faulted = false;
    __try
    {
        const uint32_t active = g_halo3ClearanceActiveMask ? *g_halo3ClearanceActiveMask : 0;
        if (active && !(active & 0xFFFF0000u))
        {
            g_halo3FreshRegionQueries.fetch_add(1, std::memory_order_relaxed);
            constexpr uint64_t flags = 9ull | (0x7FFFull << 32);
            int32_t type = 0;
            const bool interior = g_halo3ClearancePoint(flags, &center.x, ignored, -1, &type);
            const bool gathered = g_halo3ClearanceGather(flags, &center.x, radius,
                0.0f, 0.0f, ignored, -1, storage);
            uint16_t counts[3]{};
            memcpy(counts, storage, sizeof(counts));
            clear = !interior && !gathered && !counts[0] && !counts[1] && !counts[2] &&
                active == *g_halo3ClearanceActiveMask;
            for (size_t i = 0xC408; i < sizeof(storage); ++i)
                if (storage[i]) { clear = false; faulted = true; }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { faulted = true; }
    if (faulted)
    {
        g_halo3FreshRegionFaults.fetch_add(1, std::memory_order_relaxed);
        g_halo3FreshRegionEnabled.store(false, std::memory_order_release);
        return;
    }
    if (!clear || epoch != g_halo3ClearanceEpoch.load(std::memory_order_acquire))
        return;
    out.sequence.fetch_add(1, std::memory_order_acq_rel);
    out.generation.store(generation, std::memory_order_relaxed);
    out.serial.store(serial, std::memory_order_relaxed);
    out.sampleMs.store(nowMs, std::memory_order_relaxed);
    out.x.store(center.x, std::memory_order_relaxed);
    out.y.store(center.y, std::memory_order_relaxed);
    out.z.store(center.z, std::memory_order_relaxed);
    out.allowance.store(allowance, std::memory_order_relaxed);
    out.epoch.store(epoch, std::memory_order_relaxed);
    out.sequence.fetch_add(1, std::memory_order_release);
    g_halo3FreshRegionClears.fetch_add(1, std::memory_order_relaxed);
}

bool Halo3AllowFreshRegion(uint32_t generation, uint64_t approvedSerial,
    uint64_t nowMs, const BoneMatrix* approved, const BoneMatrix* proposed,
    uint32_t nodeCount)
{
    if (!g_halo3FreshRegionEnabled.load(std::memory_order_acquire) ||
        !approved || !proposed || !nodeCount || nodeCount > 16)
        return false;
    const auto& pub = g_halo3FreshRegion;
    const uint32_t seq = pub.sequence.load(std::memory_order_acquire);
    if (seq & 1u) return false;
    const uint64_t epoch = pub.epoch.load(std::memory_order_relaxed);
    const uint64_t ms = pub.sampleMs.load(std::memory_order_relaxed);
    const PhysicalContactVec3 center{pub.x.load(std::memory_order_relaxed),
        pub.y.load(std::memory_order_relaxed), pub.z.load(std::memory_order_relaxed)};
    const float allowance = pub.allowance.load(std::memory_order_relaxed);
    if (pub.generation.load(std::memory_order_relaxed) != generation ||
        pub.serial.load(std::memory_order_relaxed) != approvedSerial ||
        !PhysicalContactFreshRegionContains(epoch,
            g_halo3ClearanceEpoch.load(std::memory_order_acquire), ms, nowMs,
            center, allowance, {proposed[0].translation[0], proposed[0].translation[1], proposed[0].translation[2]}))
        return false;
    const auto transform = [](const BoneMatrix& b) {
        return PhysicalContactTransform{{b.translation[0], b.translation[1], b.translation[2]},
            {b.rotation[0], b.rotation[1], b.rotation[2]},
            {b.rotation[3], b.rotation[4], b.rotation[5]},
            {b.rotation[6], b.rotation[7], b.rotation[8]}, b.scale};
    };
    const auto oldRoot = transform(approved[0]), root = transform(proposed[0]);
    for (uint32_t i = 0; i < nodeCount; ++i)
        if (!PhysicalContactNodeRigidlyUnchanged(oldRoot, root,
                transform(approved[i]), transform(proposed[i])))
        {
            g_halo3FreshRegionShapeRejects.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    if (seq != pub.sequence.load(std::memory_order_acquire) ||
        epoch != g_halo3ClearanceEpoch.load(std::memory_order_acquire))
        return false;
    g_halo3FreshRegionFrames.fetch_add(1, std::memory_order_relaxed);
    return true;
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
    if (g_halo3FreshRegionEnabled.load() || g_halo3FreshRegionFaults.load())
        LOG("H3 fresh region EXPERIMENT status: enabled=%d queries=%llu clears=%llu frames=%llu shapeRejects=%llu faults=%llu",
            g_halo3FreshRegionEnabled.load() ? 1 : 0,
            (unsigned long long)g_halo3FreshRegionQueries.load(),
            (unsigned long long)g_halo3FreshRegionClears.load(),
            (unsigned long long)g_halo3FreshRegionFrames.load(),
            (unsigned long long)g_halo3FreshRegionShapeRejects.load(),
            (unsigned long long)g_halo3FreshRegionFaults.load());
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
