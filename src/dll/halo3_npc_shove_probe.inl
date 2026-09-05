// Opt-in simulation-context experiment. This is not production weapon contact.
// Native provenance: H3EK's named biped_accelerate dispatch -> B21690;
// pinned retail named dispatch -> 3DF6C0 -> 3DF244. Both take a biped handle
// and one vector, set the character-physics acceleration flag, then enter the
// native object acceleration path. No damage entry point is called here.
using Halo3BipedAccelerateFn = void (__fastcall*)(int32_t, const float*);
constexpr bool kEnableHalo3NpcShoveProbeCandidate = true;
Halo3BipedAccelerateFn g_halo3NpcProbeAccelerate = nullptr;
std::atomic<bool> g_halo3NpcProbeEnabled{false};
std::atomic<uint32_t> g_halo3NpcProbeStage{0}, g_halo3NpcProbeCalls{0};
std::atomic<int32_t> g_halo3NpcProbeTarget{-1};
std::atomic<float> g_halo3NpcProbeBaselineDrift{0}, g_halo3NpcProbeMoved{0};
std::atomic<float> g_halo3NpcProbeBeforeSpeed{0}, g_halo3NpcProbeAfterSpeed{0};
std::atomic<float> g_halo3NpcProbeHealth{0}, g_halo3NpcProbeShield{0};
std::atomic<float> g_halo3NpcProbeHealthLoss{0}, g_halo3NpcProbeShieldLoss{0};

void Halo3BindNpcShoveProbe(uintptr_t base, size_t size)
{
    g_halo3NpcProbeEnabled.store(false, std::memory_order_release);
    g_halo3NpcProbeStage.store(0, std::memory_order_relaxed);
    g_halo3NpcProbeAccelerate = nullptr;
    wchar_t value[8]{};
    const DWORD length = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_NPC_SHOVE", value, 8);
    if (!kEnableHalo3NpcShoveProbeCandidate || length != 1 || value[0] != L'1')
        return;
    const char* signature =
        "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 "
        "41 54 41 55 41 56 41 57 48 8D 68 A1 48 81 EC 00 01 00 00 "
        "44 8B 05 ?? ?? ?? ?? 48 8B F2 0F 29 70 C8 33 DB "
        "0F 29 78 B8 44 0F 29 40 A8";
    const uintptr_t hit = sig::Find(base, size, signature);
    if (!hit || sig::Find(hit + 1, base + size - hit - 1, signature))
    {
        g_halo3NpcProbeStage.store(6, std::memory_order_relaxed);
        LOG("H3 NPC shove PROBE: unique biped acceleration binding unavailable; probe stays off");
        return;
    }
    g_halo3NpcProbeAccelerate = reinterpret_cast<Halo3BipedAccelerateFn>(hit);
    g_halo3NpcProbeStage.store(1, std::memory_order_relaxed);
    g_halo3NpcProbeCalls.store(0, std::memory_order_relaxed);
    g_halo3NpcProbeEnabled.store(true, std::memory_order_release);
    LOG("H3 NPC shove PROBE enabled: native biped acceleration +0x%llX; simulation context; bounded sustained-cadence experiment; not weapon-contact acceptance",
        static_cast<unsigned long long>(hit - base));
}

void Halo3RunNpcShoveProbe(uint64_t nowMs)
{
    if (!g_halo3NpcProbeEnabled.load(std::memory_order_acquire) ||
        !g_halo3NpcProbeAccelerate || !g_halo3PlayerUnitGetter ||
        !g_halo3ObjectGetVelocities)
        return;
    static uint32_t sampleGeneration = 0;
    static int32_t targetHandle = -1;
    static uint64_t startMs = 0, lastPulseMs = 0;
    static PhysicalContactVec3 initial{}, pulseStart{}, direction{};
    static float initialHealth = 0, initialShield = 0;
    const uint32_t generation = g_halo3RuntimeGeneration.load(std::memory_order_acquire);
    if (!generation)
        return;
    if (generation != sampleGeneration)
    {
        sampleGeneration = generation;
        targetHandle = -1;
        startMs = lastPulseMs = 0;
    }
    bool paused = true;
    int32_t scene = -1, shot = -1;
    if (!ReadEnginePaused(paused) || paused ||
        ReadCinematicControl(scene, shot) != CinematicControlState::PlayerControlled)
        return;
    const float scale = g_worldScale.load(std::memory_order_relaxed);
    if (!std::isfinite(scale) || scale < 0.05f || scale > 2.0f)
        return;
    __try
    {
        const int32_t player = g_halo3PlayerUnitGetter(0);
        unsigned char* playerData = nullptr;
        if (!Halo3ContactObjectDataForHandle(player, playerData))
            return;
        const auto positionOf = [](const unsigned char* data) {
            const auto* p = reinterpret_cast<const float*>(data + kHalo3ObjectPositionOffset);
            return PhysicalContactVec3{p[0], p[1], p[2]};
        };
        const PhysicalContactVec3 playerPosition = positionOf(playerData);
        if (!PhysicalContactFinite(playerPosition))
            return;
        if (targetHandle == -1)
        {
            // Read only the bounded, validated native table. A held weapon is
            // deliberately unnecessary for this isolated motor experiment.
            auto** slots = reinterpret_cast<void**>(__readgsqword(0x58));
            auto* tls = slots && g_engineTlsIndex
                ? reinterpret_cast<unsigned char*>(slots[*g_engineTlsIndex]) : nullptr;
            auto* table = tls ? *reinterpret_cast<unsigned char**>(tls + kHalo3TlsObjectTableOffset) : nullptr;
            if (!table || memcmp(table, "object", 7) != 0)
                return;
            const uint32_t capacity = *reinterpret_cast<uint32_t*>(table + kOdstDataArrayMaxCountOffset);
            const uint32_t limit = *reinterpret_cast<uint32_t*>(table + kOdstDataArrayFirstUnallocatedOffset);
            if (capacity != 2048 || limit > capacity ||
                *reinterpret_cast<uint32_t*>(table + kOdstDataArrayElementSizeOffset) != kHalo3ObjectEntryStride ||
                *reinterpret_cast<uint32_t*>(table + kOdstDataArraySignatureOffset) != kOdstDataArraySignature ||
                table[kOdstDataArrayValidOffset] != 1)
                return;
            auto* entries = *reinterpret_cast<unsigned char**>(table + kHalo3ObjectTableEntriesOffset);
            if (!entries)
                return;
            float nearest = 6.0f * scale;
            for (uint32_t i = 0; i < limit; ++i)
            {
                auto* entry = entries + i * kHalo3ObjectEntryStride;
                const uint16_t salt = *reinterpret_cast<uint16_t*>(entry);
                if (salt < 0x8000 || entry[kHalo3ObjectEntryKindOffset] != 0)
                    continue;
                const int32_t candidate = static_cast<int32_t>((uint32_t{salt} << 16) | i);
                unsigned char* data = nullptr;
                if (candidate == player || !Halo3ContactObjectDataForHandle(candidate, data) ||
                    *reinterpret_cast<int32_t*>(data + kHalo3ObjectParentOffset) != -1)
                    continue;
                // H3EK unit_get_health -> AD7540; retail script body 1E3364.
                // Both test object damage flag bit 2 and then read +0xF4.
                const float health = *reinterpret_cast<float*>(data + 0xF4);
                const float shield = *reinterpret_cast<float*>(data + 0xF8);
                if ((*reinterpret_cast<uint32_t*>(data + 0x110) & 4u) ||
                    !std::isfinite(health) || health <= 0 ||
                    !std::isfinite(shield))
                    continue;
                const PhysicalContactVec3 p = positionOf(data);
                const float distance = PhysicalContactLength(p - playerPosition);
                if (!PhysicalContactFinite(p) || distance < 0.15f * scale || distance >= nearest)
                    continue;
                nearest = distance;
                targetHandle = candidate;
                initial = pulseStart = p;
                initialHealth = health;
                initialShield = shield;
            }
            if (targetHandle == -1)
                return;
            direction = initial - playerPosition;
            direction.z = 0.0f;
            direction = PhysicalContactNormalize(direction, {});
            if (PhysicalContactLengthSquared(direction) < 0.5f)
            {
                targetHandle = -1;
                return;
            }
            startMs = nowMs;
            g_halo3NpcProbeTarget.store(targetHandle, std::memory_order_relaxed);
            g_halo3NpcProbeStage.store(2, std::memory_order_relaxed);
        }
        unsigned char* targetData = nullptr;
        uint8_t kind = 0xFF;
        if (!Halo3ContactObjectDataForHandle(targetHandle, targetData, &kind) || kind != 0 || targetHandle == player)
        {
            g_halo3NpcProbeEnabled.store(false, std::memory_order_release);
            g_halo3NpcProbeStage.store(6, std::memory_order_relaxed);
            return;
        }
        const PhysicalContactVec3 position = positionOf(targetData);
        if (!PhysicalContactFinite(position))
            return;
        const float health = *reinterpret_cast<float*>(targetData + 0xF4);
        const float shield = *reinterpret_cast<float*>(targetData + 0xF8);
        g_halo3NpcProbeHealth.store(health, std::memory_order_relaxed);
        g_halo3NpcProbeShield.store(shield, std::memory_order_relaxed);
        const float healthLoss = std::max(initialHealth - health, 0.0f);
        const float shieldLoss = std::max(initialShield - shield, 0.0f);
        g_halo3NpcProbeHealthLoss.store(healthLoss, std::memory_order_relaxed);
        g_halo3NpcProbeShieldLoss.store(shieldLoss, std::memory_order_relaxed);
        if (!std::isfinite(health) || !std::isfinite(shield) ||
            health <= 0 || healthLoss > 0.001f || shieldLoss > 0.001f)
        {
            g_halo3NpcProbeStage.store(8, std::memory_order_relaxed);
            g_halo3NpcProbeEnabled.store(false, std::memory_order_release);
            return;
        }
        const uint64_t elapsed = nowMs - startMs;
        if (elapsed < 3000)
        {
            pulseStart = position;
            g_halo3NpcProbeBaselineDrift.store(PhysicalContactLength(position - initial) / scale, std::memory_order_relaxed);
            return;
        }
        g_halo3NpcProbeMoved.store(PhysicalContactLength(position - pulseStart) / scale, std::memory_order_relaxed);
        if (elapsed >= 9000)
        {
            g_halo3NpcProbeStage.store(5, std::memory_order_relaxed);
            g_halo3NpcProbeEnabled.store(false, std::memory_order_release);
            return;
        }
        if (elapsed >= 6000)
        {
            g_halo3NpcProbeStage.store(4, std::memory_order_relaxed);
            return;
        }
        g_halo3NpcProbeStage.store(3, std::memory_order_relaxed);
        if ((lastPulseMs && nowMs - lastPulseMs < 10) ||
            g_halo3NpcProbeCalls.load(std::memory_order_relaxed) >= 300)
            return;
        lastPulseMs = nowMs;
        float before[3]{}, angular[3]{};
        g_halo3ObjectGetVelocities(targetHandle, before, angular);
        const PhysicalContactVec3 v{before[0], before[1], before[2]};
        if (!PhysicalContactFinite(v) || PhysicalContactLength({v.x, v.y, 0.0f}) / scale > 0.60f)
            return;
        const PhysicalContactVec3 delta = direction * (0.15f * scale);
        const float acceleration[3] = {delta.x, delta.y, 0.0f};
        g_halo3NpcProbeBeforeSpeed.store(PhysicalContactDot(v, direction) / scale, std::memory_order_relaxed);
        g_halo3NpcProbeAccelerate(targetHandle, acceleration);
        float after[3]{};
        g_halo3ObjectGetVelocities(targetHandle, after, angular);
        const PhysicalContactVec3 afterVelocity{after[0], after[1], after[2]};
        if (PhysicalContactFinite(afterVelocity))
            g_halo3NpcProbeAfterSpeed.store(PhysicalContactDot(afterVelocity, direction) / scale, std::memory_order_relaxed);
        g_halo3NpcProbeCalls.fetch_add(1, std::memory_order_relaxed);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_halo3NpcProbeStage.store(7, std::memory_order_relaxed);
        g_halo3NpcProbeEnabled.store(false, std::memory_order_release);
    }
}

void Halo3LogNpcShoveProbe()
{
    if (!g_halo3NpcProbeStage.load(std::memory_order_relaxed))
        return;
    LOG("H3 NPC shove PROBE: stage=%u target=0x%08X calls=%u baselineDrift=%.5fm moved=%.5fm before=%.5fm/s after=%.5fm/s health=%.5f shield=%.5f healthLoss=%.5f shieldLoss=%.5f (native motor experiment; no melee call)",
        g_halo3NpcProbeStage.load(std::memory_order_relaxed),
        static_cast<uint32_t>(g_halo3NpcProbeTarget.load(std::memory_order_relaxed)),
        g_halo3NpcProbeCalls.load(std::memory_order_relaxed),
        g_halo3NpcProbeBaselineDrift.load(std::memory_order_relaxed),
        g_halo3NpcProbeMoved.load(std::memory_order_relaxed),
        g_halo3NpcProbeBeforeSpeed.load(std::memory_order_relaxed),
        g_halo3NpcProbeAfterSpeed.load(std::memory_order_relaxed),
        g_halo3NpcProbeHealth.load(std::memory_order_relaxed),
        g_halo3NpcProbeShield.load(std::memory_order_relaxed),
        g_halo3NpcProbeHealthLoss.load(std::memory_order_relaxed),
        g_halo3NpcProbeShieldLoss.load(std::memory_order_relaxed));
}
