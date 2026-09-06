// Opt-in, two-record numerical capture for rejected null-controller contacts.
// Simulation publishes once; the cold status logger drains each complete slot.
struct Halo3ContactReplayRecord
{
    uint64_t timeMs = 0;
    int32_t weapon = -1, target = -1;
    uint32_t source = 0;
    float fraction = 0, worldScale = 0;
    PhysicalContactTransform previous{}, intended{}, targetTransform{};
    PhysicalContactConvexShape weaponShape{}, targetShape{};
};
std::array<Halo3ContactReplayRecord, 2> g_halo3ContactReplayRecords{};
std::array<std::atomic<uint32_t>, 2> g_halo3ContactReplayStates{};

void Halo3CaptureContactReplay(uint64_t timeMs, int32_t weapon, int32_t target,
    uint32_t source, float fraction, float worldScale,
    const PhysicalContactTransform& previous,
    const PhysicalContactTransform& intended,
    const PhysicalContactTransform& targetTransform,
    const PhysicalContactConvexShape& weaponShape,
    const PhysicalContactConvexShape& targetShape)
{
    if (!VR_UsesFixedControllerDebugPose()) return;
    for (size_t i = 0; i < g_halo3ContactReplayStates.size(); ++i)
    {
        uint32_t expected = 0;
        if (!g_halo3ContactReplayStates[i].compare_exchange_strong(
                expected, 1, std::memory_order_acquire)) continue;
        auto& r = g_halo3ContactReplayRecords[i];
        r.timeMs = timeMs; r.weapon = weapon; r.target = target;
        r.source = source; r.fraction = fraction; r.worldScale = worldScale;
        r.previous = previous; r.intended = intended;
        r.targetTransform = targetTransform;
        r.weaponShape = weaponShape; r.targetShape = targetShape;
        g_halo3ContactReplayStates[i].store(2, std::memory_order_release);
        break;
    }
}

void Halo3LogContactReplays()
{
    for (size_t i = 0; i < g_halo3ContactReplayStates.size(); ++i)
    {
        uint32_t expected = 2;
        if (!g_halo3ContactReplayStates[i].compare_exchange_strong(
                expected, 3, std::memory_order_acquire)) continue;
        const auto& r = g_halo3ContactReplayRecords[i];
        LOG("H3 contact replay id=%u meta: ms=%llu weapon=0x%08X target=0x%08X source=%u fraction=%.9g worldScale=%.9g",
            (unsigned)i, (unsigned long long)r.timeMs, (unsigned)r.weapon,
            (unsigned)r.target, r.source, r.fraction, r.worldScale);
        const auto logTransform = [&](const char* name, const PhysicalContactTransform& t) {
            LOG("H3 contact replay id=%u transform %s: %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",
                (unsigned)i, name, t.position.x, t.position.y, t.position.z,
                t.forward.x, t.forward.y, t.forward.z, t.left.x, t.left.y, t.left.z,
                t.up.x, t.up.y, t.up.z, t.scale);
        };
        logTransform("previous", r.previous);
        logTransform("intended", r.intended);
        logTransform("target", r.targetTransform);
        const auto logShape = [&](const char* name, const PhysicalContactConvexShape& s) {
            LOG("H3 contact replay id=%u shape %s: count=%u radius=%.9g",
                (unsigned)i, name, (unsigned)s.vertexCount, s.radius);
            for (uint16_t v = 0; v < std::min<size_t>(s.vertexCount, s.vertices.size()); ++v)
                LOG("H3 contact replay id=%u vertex %s %u: %.9g %.9g %.9g",
                    (unsigned)i, name, (unsigned)v,
                    s.vertices[v].x, s.vertices[v].y, s.vertices[v].z);
        };
        logShape("weapon", r.weaponShape);
        logShape("target", r.targetShape);
        LOG("H3 contact replay id=%u complete", (unsigned)i);
    }
}
