// Diagnostic only. Native selection count leaf and its first-person caller:
// docs/HALO3-SWORD-CONTACT-EVIDENCE.md, retail-selection-verification.json.
// The leaf reads ECX/R8, overwrites EDX before use, reads no stack arguments,
// and returns the count in EAX. Forward the unused register argument unchanged.
using Halo3SelectionCountFn = int32_t(__fastcall*)(uint32_t, uintptr_t, const uint16_t*);
Halo3SelectionCountFn g_halo3SelectionCountOriginal = nullptr;
std::atomic<uintptr_t> g_halo3SelectionProbeCaller{0};
std::atomic<const unsigned char*> g_halo3SelectionProbePalette{nullptr};
std::atomic<bool> g_halo3SelectionProbeEnabled{false};
std::atomic<uint64_t> g_halo3SelectionProbeCalls{0}, g_halo3SelectionProbeFaults{0};
std::atomic<uint64_t> g_halo3SelectionProbeRejected{0}, g_halo3SelectionProbeReservations{0};
struct Halo3SelectionProbeKey
{
    uint32_t generation = 0, tag = 0;
    int32_t weapon = -1, regions = 0, nodes = 0;
    uint16_t meshes[16]{};
};
struct Halo3SelectionProbeRecord
{
    Halo3SelectionProbeKey key{};
    uint64_t ms = 0, drawnSerial = 0;
    int32_t matrices = 0;
    bool matchesDrawn = false;
    BoneMatrix nodes[16]{};
};
constexpr size_t kHalo3SelectionProbeRecords = 64;
Halo3SelectionProbeRecord g_halo3SelectionProbeRecords[kHalo3SelectionProbeRecords]{};
std::atomic<uint32_t> g_halo3SelectionProbeStates[kHalo3SelectionProbeRecords]{};

void Halo3ObserveSelection(uint32_t tag, const uint16_t* meshes, int32_t matrixCount)
{
    __try
    {
        const auto* palette = g_halo3SelectionProbePalette.load(std::memory_order_acquire);
        if (!meshes || !palette ||
            tag != g_halo3ContactPreparedRenderDatum.load(std::memory_order_relaxed))
            return;
        g_halo3SelectionProbeCalls.fetch_add(1, std::memory_order_relaxed);
        const auto* definition = Halo3LoadedTagDefinition(tag);
        if (!definition) return;
        Halo3SelectionProbeKey key{};
        key.generation = g_halo3RuntimeGeneration.load(std::memory_order_acquire);
        key.tag = tag;
        key.regions = *reinterpret_cast<const int32_t*>(definition + 0x0C);
        key.nodes = *reinterpret_cast<const int32_t*>(definition + 0x30);
        const int32_t paletteCount = *reinterpret_cast<const int32_t*>(palette - 4);
        if (!key.generation || key.regions < 1 || key.regions > 16 ||
            key.nodes < 1 || key.nodes > 16 || paletteCount < 1 || paletteCount > 4)
        {
            g_halo3SelectionProbeRejected.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const unsigned char* entry = nullptr;
        for (int i = 0; i < paletteCount; ++i)
        {
            const auto* candidate = palette + i * 0xD0C;
            if (*reinterpret_cast<const uint32_t*>(candidate) != tag) continue;
            if (entry) // Same render tag on multiple objects is not unique identity.
            {
                g_halo3SelectionProbeRejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            entry = candidate;
        }
        if (!entry) return;
        key.weapon = *reinterpret_cast<const int32_t*>(entry + 4);
        memcpy(key.meshes, meshes, key.regions * sizeof(uint16_t));
        // Keep the first selection and its first exact published-palette match.
        // A switch's first palette may not match the contact publication;
        // selection-only deduplication would hide every later settled match.
        static thread_local Halo3SelectionProbeKey previous{};
        static thread_local bool previousPaired = false;
        const bool sameSelection = !memcmp(&previous, &key, sizeof(key));
        if (sameSelection && previousPaired) return;
        Halo3SelectionProbeRecord record{};
        record.key = key;
        record.ms = GetTickCount64();
        record.matrices = matrixCount;
        memcpy(record.nodes, entry + 12, key.nodes * sizeof(BoneMatrix));
        for (int n = 0; n < key.nodes; ++n)
        {
            const auto& node = record.nodes[n];
            bool finite = std::isfinite(node.scale);
            for (float v : node.rotation) finite = finite && std::isfinite(v);
            for (float v : node.translation) finite = finite && std::isfinite(v);
            if (!finite)
            {
                g_halo3SelectionProbeRejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
        BoneMatrix drawn[Halo3VisibleWeaponPosePublication::kMaximumNodes]{};
        float basis[9]{}, position[3]{}, scale = 1;
        uint64_t ms = 0, serial = 0;
        uint32_t nodeCount = 0;
        uint16_t drawnTag = 0xFFFF;
        int32_t weapon = -1;
        if (Halo3ReadWeaponPose(g_halo3LastDrawnWeaponPose, basis, position, scale, ms,
                drawn, &nodeCount, &drawnTag, &weapon, &serial) &&
            nodeCount == static_cast<uint32_t>(key.nodes) && drawnTag == static_cast<uint16_t>(tag) &&
            weapon == key.weapon && !memcmp(drawn, record.nodes, key.nodes * sizeof(BoneMatrix)))
        {
            record.matchesDrawn = true;
            record.drawnSerial = serial;
        }
        if (sameSelection && !record.matchesDrawn) return;
        previous = key;
        previousPaired = record.matchesDrawn;
        const uint64_t index = g_halo3SelectionProbeReservations.fetch_add(1, std::memory_order_relaxed);
        if (index >= kHalo3SelectionProbeRecords) return;
        g_halo3SelectionProbeRecords[index] = record;
        g_halo3SelectionProbeStates[index].store(2, std::memory_order_release);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_halo3SelectionProbeFaults.fetch_add(1, std::memory_order_relaxed);
        // Disable observation only. The native function still executes on every call.
        g_halo3SelectionProbeEnabled.store(false, std::memory_order_release);
    }
}

int32_t __fastcall Halo3SelectionCountProbe(uint32_t tag, uintptr_t unused, const uint16_t* meshes)
{
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const int32_t result = g_halo3SelectionCountOriginal(tag, unused, meshes);
    if (caller == g_halo3SelectionProbeCaller.load(std::memory_order_acquire) &&
        g_halo3SelectionProbeEnabled.load(std::memory_order_acquire))
        Halo3ObserveSelection(tag, meshes, result);
    return result;
}

void Halo3LogSelectionProbe()
{
    if (!g_halo3SelectionProbeCaller.load(std::memory_order_acquire)) return;
    for (size_t i = 0; i < kHalo3SelectionProbeRecords; ++i)
    {
        uint32_t ready = 2;
        if (!g_halo3SelectionProbeStates[i].compare_exchange_strong(ready, 3, std::memory_order_acquire))
            continue;
        const auto& r = g_halo3SelectionProbeRecords[i];
        char meshes[160]{};
        size_t used = 0;
        for (int m = 0; m < r.key.regions; ++m)
            used += snprintf(meshes + used, sizeof(meshes) - used, "%s%u", m ? "," : "", r.key.meshes[m]);
        LOG("H3 selection PROBE record: index=%u ms=%llu generation=%u tag=0x%08X weapon=0x%08X regions=%d nodes=%d matrices=%d meshes=[%s] matchesDrawn=%d drawnSerial=%llu",
            (unsigned)i, (unsigned long long)r.ms, r.key.generation, r.key.tag, (unsigned)r.key.weapon,
            r.key.regions, r.key.nodes, r.matrices, meshes, r.matchesDrawn, (unsigned long long)r.drawnSerial);
        for (int n = 0; n < r.key.nodes; ++n)
        {
            const auto& b = r.nodes[n];
            LOG("H3 selection PROBE node: index=%u node=%d matrix=%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",
                (unsigned)i, n, b.scale, b.rotation[0], b.rotation[1], b.rotation[2],
                b.rotation[3], b.rotation[4], b.rotation[5], b.rotation[6], b.rotation[7], b.rotation[8],
                b.translation[0], b.translation[1], b.translation[2]);
        }
    }
    LOG("H3 selection PROBE status: enabled=%d calls=%llu changes=%llu rejected=%llu faults=%llu",
        g_halo3SelectionProbeEnabled.load(), (unsigned long long)g_halo3SelectionProbeCalls.load(),
        (unsigned long long)g_halo3SelectionProbeReservations.load(),
        (unsigned long long)g_halo3SelectionProbeRejected.load(), (unsigned long long)g_halo3SelectionProbeFaults.load());
}

void Halo3InstallSelectionProbe(uintptr_t base, size_t size)
{
    wchar_t flag[2]{};
    if (GetEnvironmentVariableW(L"HALOMCCVR_H3_SWORD_SELECTION_PROBE", flag, 2) != 1 || flag[0] != L'1') return;
    const auto unique = [=](const char* pattern) {
        const uintptr_t hit = sig::Find(base, size, pattern);
        return hit && !sig::Find(hit + 1, base + size - hit - 1, pattern) ? hit : uintptr_t{0};
    };
    const uintptr_t leaf = unique("48 89 5C 24 08 48 8B 05 ?? ?? ?? ?? 4D 8B D8 48 8B 1D ?? ?? ?? ?? 0F B7 C9 8B 54 C8 04");
    const uintptr_t caller = unique("41 8B 0C 24 48 8D 7B 0E 4C 8B C7 E8 ?? ?? ?? ?? 45 33 FF 88 43 0C");
    const uintptr_t producer = unique("4C 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 05 ?? ?? ?? ?? 8B 0D");
    const uintptr_t consumer = unique("4C 8D 25 ?? ?? ?? ?? 4C 8B 05 ?? ?? ?? ?? 4C 8B D3 44 8B 6C 24 48 48 89 5C 24 60 41 0F B7 04 24");
    if (!leaf || !caller || !producer || !consumer)
    {
        LOG("H3 selection PROBE unavailable: missing/ambiguous signature; camera/contact unchanged");
        return;
    }
    const uintptr_t callTarget = caller + 16 + *reinterpret_cast<const int32_t*>(caller + 12);
    const uintptr_t palette = producer + 7 + *reinterpret_cast<const int32_t*>(producer + 3);
    const uintptr_t count = producer + 18 + *reinterpret_cast<const int32_t*>(producer + 14);
    const uintptr_t renderPalette = consumer + 7 + *reinterpret_cast<const int32_t*>(consumer + 3);
    if (callTarget != leaf || palette != renderPalette || count + 4 != palette ||
        palette < base + 4 || size < 4 * 0xD0C || palette - base > size - 4 * 0xD0C)
    {
        LOG("H3 selection PROBE unavailable: caller/palette relation failed; camera/contact unchanged");
        return;
    }
    void* target = reinterpret_cast<void*>(leaf);
    if (MH_CreateHook(target, reinterpret_cast<void*>(&Halo3SelectionCountProbe),
            reinterpret_cast<void**>(&g_halo3SelectionCountOriginal)) != MH_OK)
    {
        LOG("H3 selection PROBE unavailable: hook creation failed; camera/contact unchanged");
        return;
    }
    g_halo3SelectionProbeCaller = caller + 16;
    g_halo3SelectionProbePalette = reinterpret_cast<const unsigned char*>(palette);
    g_halo3SelectionProbeEnabled.store(true, std::memory_order_release);
    RememberInstalledGameHook(target);
    if (MH_EnableHook(target) != MH_OK)
    {
        g_halo3SelectionProbeEnabled.store(false, std::memory_order_release);
        LOG("H3 selection PROBE unavailable: hook enable failed; camera/contact unchanged");
        return;
    }
    LOG("H3 selection PROBE installed at +0x%llX caller=+0x%llX palette=+0x%llX; capture only, no collision changes",
        (unsigned long long)(leaf-base), (unsigned long long)(caller-base), (unsigned long long)(palette-base));
}
