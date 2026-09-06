// Opt-in native observations and experimental empty-region tracking.
// Evidence and remaining coverage limitations: HALO3-CLEARANCE-QUERY-EVIDENCE.md.
using Halo3ClearancePointFn = bool(__fastcall*)(uint64_t, const float*, int32_t, int32_t, int32_t*);
using Halo3ClearanceGatherFn = bool(__fastcall*)(uint64_t, const float*, float, float,
                                               float, int32_t, int32_t, void*);
Halo3ClearancePointFn g_halo3ClearancePoint = nullptr;
Halo3ClearanceGatherFn g_halo3ClearanceGather = nullptr;
using Halo3VolumeSolveFn = uint16_t(__fastcall*)(const float*, const float*, const void*,
    float*, float*, uint16_t, void*);
Halo3VolumeSolveFn g_halo3VolumeSolve = nullptr;
using Halo3VolumeFirstHitFn = bool(__fastcall*)(const void*, const float*, const float*, void*);
Halo3VolumeFirstHitFn g_halo3VolumeFirstHit = nullptr;
std::atomic<bool> g_halo3VolumeProbeEnabled{false};
std::atomic<uint64_t> g_halo3SeedFeatureClear{0},g_halo3SeedFeatureRejected{0};
struct Halo3VolumeProbeRecord
{
    uint64_t ms{};
    uint32_t mode{};
    int32_t surfaceType{};
    float radius{}, clearance{};
    PhysicalContactVec3 start{}, motion{}, result{}, velocity{};
    uint16_t featureCounts[3]{}, collisions{};
    bool interior{}, gathered{}, bounded{}, faulted{};
    double microseconds{};
    bool coverBuilt{}, coverSeed{}, coverValid{}, coverBlocked{}, coverExhausted{};
    uint16_t coverCount{};
    uint32_t coverQueries{};
    float coverProgress{}, coverClearance{};
    double coverMicroseconds{};
};
std::array<Halo3VolumeProbeRecord, 32> g_halo3VolumeProbeRecords{};
std::array<std::atomic<uint32_t>, 32> g_halo3VolumeProbeStates{};
std::atomic<bool> g_halo3ClearanceProbeEnabled{false};
std::atomic<bool> g_halo3FreshRegionEnabled{false};
const uint32_t* g_halo3ClearanceActiveMask = nullptr;
std::atomic<uint64_t> g_halo3FreshRegionQueries{0}, g_halo3FreshRegionClears{0};
std::atomic<uint64_t> g_halo3FreshRegionFrames{0}, g_halo3FreshRegionFaults{0};
std::atomic<uint64_t> g_halo3FreshRegionShapeRejects{0};
struct Halo3FreshRegionPublication
{
    std::atomic<uint32_t> sequence{0}, generation{0};
    std::atomic<bool> objectOnly{false};
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
    g_halo3VolumeProbeEnabled.store(false, std::memory_order_release);
    g_halo3WorldVolumeEnabled.store(false, std::memory_order_release);
    g_halo3WorldGatherOnly.store(false, std::memory_order_release);
    g_halo3WorldPartitions.store(false, std::memory_order_release);
    g_halo3WorldMeshAuditEnabled.store(false, std::memory_order_release);
    g_halo3ClearanceActiveMask = nullptr;
    wchar_t value[2]{};
    const bool probeRequested = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_CLEARANCE", value, 2) == 1 && value[0] == L'1';
    const bool volumeRequested = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_VOLUME", value, 2) == 1 && value[0] == L'1';
    // Disabled after the full sword/Floodgate rock run: recurring unknown
    // gathers/casts held and hid the weapon. Retain the implementation while
    // independent observations establish the failing boundary.
    constexpr bool kEnableHalo3WorldVolumeExperiment = false;
    const bool worldFlag = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_WORLD_VOLUME", value, 2) == 1 && value[0] == L'1';
    // Opt-in bounded depenetration candidate for rejected changed-cover seeds.
    constexpr bool kEnableHalo3WorldPartitionsExperiment = true;
    const bool partitionsFlag = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_WORLD_PARTITIONS", value, 2) == 1 && value[0] == L'1';
    const bool partitions = kEnableHalo3WorldPartitionsExperiment && partitionsFlag;
    if (partitionsFlag && !kEnableHalo3WorldPartitionsExperiment)
        LOG("H3 world partitions EXPERIMENT disabled after Guardian sword reset recovery failure; prior contact path retained; VR unchanged");
    const bool worldRequested = (kEnableHalo3WorldVolumeExperiment && worldFlag) || partitions;
    const bool meshAuditRequested = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_WORLD_MESH", value, 2) == 1 && value[0] == L'1';
    if (worldFlag && !kEnableHalo3WorldVolumeExperiment && !partitions)
        LOG("H3 world volume EXPERIMENT disabled after Floodgate sword gather/cast failures; prior contact path retained; VR unchanged");
    const bool gatherOnly = GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_DEBUG_WORLD_GATHER", value, 2) == 1 && value[0] == L'1';
    constexpr bool kEnableHalo3FreshRegionExperiment = true;
    const bool freshRequested = worldRequested || (kEnableHalo3FreshRegionExperiment && GetEnvironmentVariableW(
        L"HALOMCCVR_H3_CONTACT_FRESH_REGION", value, 2) == 1 && value[0] == L'1');
    if (!probeRequested && !freshRequested && !volumeRequested && !gatherOnly)
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
    if (freshRequested || volumeRequested || gatherOnly)
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
            g_halo3FreshRegionEnabled.store(freshRequested, std::memory_order_release);
        }
        if (freshRequested)
        LOG("H3 fresh region EXPERIMENT: %s; full shape bound + 0.30m motion + 0.25m reserve; 20ms/one object epoch; native-query coverage remains under test",
            g_halo3FreshRegionEnabled.load() ? "enabled" : "disabled: active structure binding unavailable");
    }
    if ((volumeRequested || worldRequested || gatherOnly) && g_halo3ClearanceActiveMask)
    {
        const uintptr_t solver = unique("48 8B C4 4C 89 40 18 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 38 FF FF FF 48 81 EC 88 01 00 00");
        const uintptr_t firstHit = unique("48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 54 41 55 41 56 41 57 48 8B EC 48 81 EC 80 00 00 00 0F 29 70 C8 4D 8B F0 F3 0F 10 35 97 BC 5F 00 4C 8B FA");
        if (solver && solver - base == 0x1FEF30 && firstHit && firstHit-base == 0x24B8B0)
        {
            g_halo3VolumeSolve = reinterpret_cast<Halo3VolumeSolveFn>(solver);
            g_halo3VolumeFirstHit = reinterpret_cast<Halo3VolumeFirstHitFn>(firstHit);
            g_halo3VolumeProbeEnabled.store(volumeRequested, std::memory_order_release);
            g_halo3WorldGatherOnly.store(gatherOnly, std::memory_order_release);
            g_halo3WorldPartitions.store(partitions, std::memory_order_release);
            g_halo3WorldMeshAuditEnabled.store(worldRequested && !gatherOnly && meshAuditRequested,
                std::memory_order_release);
            g_halo3WorldVolumeEnabled.store(worldRequested || gatherOnly, std::memory_order_release);
            if (gatherOnly)
                LOG("H3 world gather PROBE enabled: raw-pose worker observations only; no world pose ownership, no mesh audit, no weapon hiding");
            if (volumeRequested)
                LOG("H3 volume PROBE enabled: 32 native sphere-motion observations; no production pose changes");
            if (worldRequested)
                LOG("H3 world volume EXPERIMENT enabled: worker geometry, current-pose rigid sweep/slide, 30cm clear-only recovery; headset acceptance pending");
            if (partitions)
                LOG("H3 world partitions EXPERIMENT enabled: swept-sphere regions, exact expansion and capsule containment, native capacity guards retained");
            if (worldRequested)
                LOG("H3 world mesh AUDIT mode: enabled=%d; diagnostic only, no solver permission; audit timings are not ordinary tracking timings",
                    g_halo3WorldMeshAuditEnabled.load() ? 1 : 0);
        }
        else LOG("H3 volume PROBE disabled: unique verified solver unavailable; VR unchanged");
    }
}

// The caller must own and validate the immutable features for this entire call.
// This function performs only the audited math query, with no world gathering,
// active-mask access or player/object lookup.
PhysicalContactVolumeCast Halo3CastVolumeFeatures(const void* features,
    PhysicalContactVec3 start,PhysicalContactVec3 motion)
{
    PhysicalContactVolumeCast result{};
    if (!features || !g_halo3VolumeFirstHit || !PhysicalContactFinite(start) ||
        !PhysicalContactFinite(motion)) return result;
    alignas(16) unsigned char record[48+64];
    memset(record,0xCD,sizeof(record));
    __try
    {
        result.hit=g_halo3VolumeFirstHit(features,&start.x,&motion.x,record);
        memcpy(&result.fraction,record+0x10,4);
        PhysicalContactVec3 point{}; memcpy(&point,record+0x14,sizeof(point));
        if (!std::isfinite(result.fraction) || result.fraction<0 || result.fraction>1 ||
            !PhysicalContactFinite(point) ||
            PhysicalContactLength(point-(start+motion*result.fraction))>.001f) return {};
        if (result.hit)
        {
            memcpy(&result.normal,record+0x20,sizeof(result.normal));
            float distance=0; memcpy(&distance,record+0x2C,4);
            if (!PhysicalContactFinite(result.normal) || !std::isfinite(distance) ||
                std::abs(PhysicalContactLengthSquared(result.normal)-1)>.002f) return {};
        }
        for (size_t i=48;i<sizeof(record);++i) if (record[i]!=0xCD) return {};
        result.valid=true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return {}; }
    return result;
}

// Worker-only gather adapter. Explicit owner exclusion is required because the
// retail convenience movement wrapper drops it. Evidence and record layout:
// HALO3-NATIVE-VOLUME-EVIDENCE.md.
PhysicalContactVolumeCast Halo3CastNativeVolume(PhysicalContactVec3 start,
    PhysicalContactVec3 motion, float radius, int32_t ignored, uint64_t flags)
{
    PhysicalContactVolumeCast result{};
    if (!g_halo3VolumeFirstHit || !g_halo3ClearanceGather || !g_halo3ClearanceActiveMask ||
        !PhysicalContactFinite(start) || !PhysicalContactFinite(motion) ||
        !std::isfinite(radius) || radius <= 0 || radius > 10) return result;
    const auto center=start+motion*.5f;
    const float searchRadius=radius+PhysicalContactLength(motion)*.5f;
    if (!PhysicalContactFinite(center) || !std::isfinite(searchRadius) || searchRadius>20) return result;
    alignas(16) unsigned char features[0xC490];
    memset(features,0xCD,sizeof(features));
    __try
    {
        const uint32_t active=*g_halo3ClearanceActiveMask;
        if (!active || (active&0xFFFF0000u)) return result;
        const bool gathered=g_halo3ClearanceGather(flags,&center.x,searchRadius,0,radius,ignored,-1,features);
        uint16_t counts[3]{}; memcpy(counts,features,sizeof(counts));
        // A full feature category may be truncated. It is not a clear result.
        if (!Halo3VolumeFeaturesValid(features,sizeof(features))) return result;
        for (size_t i=0xC408;i<sizeof(features);++i) if (features[i]!=0xCD) return result;
        if (gathered)
        {
            result=Halo3CastVolumeFeatures(features,start,motion);
            if (!result.valid) return result;
        }
        else if (counts[0] || counts[1] || counts[2]) return {};
        if (active!=*g_halo3ClearanceActiveMask) return {};
        result.valid=true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return {}; }
    return result;
}

// Prove the center outside native solid interiors, then outside every feature
// expanded by this sphere's radius. Nearby features alone are not overlap.
// First-hit with zero motion cannot replace point membership: it filters by
// movement direction. All native work remains on the simulation worker.
bool Halo3NativeVolumeSeedClear(PhysicalContactVec3 center,float radius,int32_t ignored,uint64_t flags,
    uint32_t expectedActive=0)
{
    if (!g_halo3ClearancePoint || !g_halo3ClearanceGather || !g_halo3ClearanceActiveMask ||
        !PhysicalContactFinite(center) || !std::isfinite(radius) || radius<=0 || radius>10) return false;
    alignas(16) unsigned char features[0xC490];
    memset(features,0xCD,sizeof(features));
    __try
    {
        const uint32_t active=*g_halo3ClearanceActiveMask;
        if (!active || (active&0xFFFF0000u) || (expectedActive && active!=expectedActive)) return false;
        int32_t type=0;
        if (g_halo3ClearancePoint(flags,&center.x,ignored,-1,&type)) return false;
        const bool gathered=g_halo3ClearanceGather(flags,&center.x,radius,0,radius,ignored,-1,features);
        uint16_t counts[3]{}; memcpy(counts,features,sizeof(counts));
        const bool nonempty=counts[0] || counts[1] || counts[2];
        if ((!gathered && nonempty) || !Halo3VolumeFeaturesValid(features,sizeof(features))) return false;
        for (size_t i=0xC408;i<sizeof(features);++i) if (features[i]!=0xCD) return false;
        const bool clear=Halo3VolumePointOutsideFeatures(features,sizeof(features),&center.x);
        if (active!=*g_halo3ClearanceActiveMask) return false;
        if (nonempty) (clear ? g_halo3SeedFeatureClear : g_halo3SeedFeatureRejected).fetch_add(1,std::memory_order_relaxed);
        return clear;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

#include "halo3_world_volume.inl"

void Halo3ProbeWeaponVolume(Halo3VolumeProbeRecord& r,
    const PhysicalContactCompoundShape& shape, PhysicalContactTransform from,
    PhysicalContactVec3 surface, PhysicalContactVec3 normal,float worldScale,int32_t ignored)
{
    LARGE_INTEGER begin{},end{},frequency{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&begin);
    PhysicalContactVolumeCover cover{};
    r.coverBuilt=PhysicalContactVolumeRigid(from) &&
        PhysicalContactBuildVolumeCover(shape,.12f*worldScale/from.scale,cover);
    if (!r.coverBuilt) return;
    r.coverCount=cover.count;
    const float skin=.005f*worldScale;
    float minimum=FLT_MAX;
    for (uint16_t i=0;i<cover.count;++i)
        minimum=std::min(minimum,PhysicalContactDot(
            PhysicalContactTransformPoint(from,cover.spheres[i].center)-surface,normal)-
            cover.spheres[i].radius*from.scale-skin);
    from.position=from.position+normal*(.60f*worldScale-minimum);
    constexpr uint64_t flags=9ull|(0x7FFFull<<32);
    r.coverSeed=true;
    for (uint16_t i=0;i<cover.count && r.coverSeed;++i)
        r.coverSeed=Halo3NativeVolumeSeedClear(PhysicalContactTransformPoint(from,cover.spheres[i].center),
            cover.spheres[i].radius*from.scale+skin,ignored,flags);
    if (r.coverSeed)
    {
        auto to=from;
        to.position=to.position+normal*((r.mode==1 ? .20f : -.90f)*worldScale);
        if (r.mode==2)
        {
            const auto tangent=PhysicalContactNormalize(PhysicalContactCross(normal,
                std::abs(normal.z)<.9f ? PhysicalContactVec3{0,0,1}:PhysicalContactVec3{0,1,0}));
            to.position=to.position+tangent*(.40f*worldScale);
        }
        if (r.mode==3)
        {
            to.forward=PhysicalContactRotateAxisAngle(from.forward,{0,0,1},1.5707963f);
            to.left=PhysicalContactRotateAxisAngle(from.left,{0,0,1},1.5707963f);
            to.up=PhysicalContactRotateAxisAngle(from.up,{0,0,1},1.5707963f);
        }
        const auto swept=PhysicalContactSweepVolume(cover,from,to,skin,192,
            [&](PhysicalContactVec3 start,PhysicalContactVec3 motion,float radius) {
                return Halo3CastNativeVolume(start,motion,radius,ignored,flags);
            });
        r.coverValid=swept.valid; r.coverBlocked=swept.blocked; r.coverExhausted=swept.exhausted;
        r.coverQueries=swept.queries; r.coverProgress=swept.progress;
        minimum=FLT_MAX;
        for (uint16_t i=0;i<cover.count;++i)
            minimum=std::min(minimum,PhysicalContactDot(
                PhysicalContactTransformPoint(swept.pose,cover.spheres[i].center)-surface,normal)-
                cover.spheres[i].radius*swept.pose.scale);
        r.coverClearance=minimum/worldScale;
    }
    QueryPerformanceCounter(&end);
    if (frequency.QuadPart>0) r.coverMicroseconds=(end.QuadPart-begin.QuadPart)*1.e6/frequency.QuadPart;
}

void Halo3RunVolumeProbe(uint64_t nowMs, PhysicalContactVec3 surface,
    PhysicalContactVec3 normal, PhysicalContactVec3 camera, int32_t surfaceType,
    float worldScale, int32_t ignoredPlayer,
    const PhysicalContactCompoundShape& shape, const PhysicalContactTransform& weapon)
{
    if (!g_halo3VolumeProbeEnabled.load(std::memory_order_acquire) ||
        !PhysicalContactFinite(surface) || !PhysicalContactFinite(normal) ||
        !PhysicalContactFinite(camera) || !std::isfinite(worldScale) ||
        worldScale < .05f || worldScale > 2.0f || PhysicalContactLengthSquared(normal) < .5f)
        return;
    static uint32_t next = 0;
    static uint64_t lastMs = 0;
    if (next >= g_halo3VolumeProbeRecords.size() || nowMs < lastMs || nowMs - lastMs < 250)
        return;
    normal = PhysicalContactNormalize(normal);
    if (PhysicalContactDot(normal, camera - surface) < 0) normal = normal * -1.0f;
    const uint32_t index = next++;
    lastMs = nowMs;
    auto& r = g_halo3VolumeProbeRecords[index];
    r.ms = nowMs;
    r.mode = index % 4;
    r.surfaceType = surfaceType;
    constexpr float radii[4]{.03f, .08f, .16f, .24f};
    r.radius = radii[(index / 4) % 4] * worldScale;
    r.start = surface + normal * ((r.mode == 3 ? -.02f : .60f) * worldScale);
    r.motion = normal * ((r.mode == 1 ? .20f : -.90f) * worldScale);
    if (r.mode == 2)
    {
        const auto tangent = PhysicalContactNormalize(PhysicalContactCross(normal,
            std::abs(normal.z) < .9f ? PhysicalContactVec3{0,0,1} : PhysicalContactVec3{0,1,0}));
        r.motion = r.motion + tangent * (.40f * worldScale);
    }
    const auto center = r.start + r.motion * .5f;
    const float gatherRadius = PhysicalContactLength(r.motion) * .5f + r.radius;
    alignas(16) unsigned char features[0xC490];
    alignas(16) unsigned char collisions[16 * 48 + 64];
    memset(features, 0xCD, sizeof(features));
    memset(collisions, 0xCD, sizeof(collisions));
    r.result = r.start + r.motion;
    r.velocity = r.motion;
    LARGE_INTEGER begin{}, end{}, frequency{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&begin);
    __try
    {
        const uint32_t active = *g_halo3ClearanceActiveMask;
        if (active && !(active & 0xFFFF0000u))
        {
            constexpr uint64_t flags = 9ull | (0x7FFFull << 32);
            int32_t pointType = 0;
            r.interior = g_halo3ClearancePoint(flags, &r.start.x, ignoredPlayer, -1, &pointType);
            r.gathered = g_halo3ClearanceGather(flags, &center.x, gatherRadius,
                0.0f, r.radius, ignoredPlayer, -1, features);
            memcpy(r.featureCounts, features, sizeof(r.featureCounts));
            r.bounded = r.featureCounts[0] <= 256 && r.featureCounts[1] <= 256 && r.featureCounts[2] <= 256;
            for (size_t i = 0xC408; i < sizeof(features); ++i) r.bounded = r.bounded && features[i] == 0xCD;
            if (r.gathered && r.bounded)
                r.collisions = g_halo3VolumeSolve(&r.start.x, &r.motion.x, features,
                    &r.result.x, &r.velocity.x, 16, collisions);
            r.bounded = r.bounded && r.collisions <= 16 && PhysicalContactFinite(r.result) &&
                PhysicalContactFinite(r.velocity) && active == *g_halo3ClearanceActiveMask;
            for (size_t i = 0xC408; i < sizeof(features); ++i) r.bounded = r.bounded && features[i] == 0xCD;
            for (size_t i = 16 * 48; i < sizeof(collisions); ++i) r.bounded = r.bounded && collisions[i] == 0xCD;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { r.faulted = true; }
    QueryPerformanceCounter(&end);
    if (frequency.QuadPart > 0) r.microseconds = (end.QuadPart - begin.QuadPart) * 1.0e6 / frequency.QuadPart;
    r.clearance = PhysicalContactDot(r.result - surface, normal) / worldScale;
    if (!r.faulted && r.bounded)
        Halo3ProbeWeaponVolume(r,shape,weapon,surface,normal,worldScale,ignoredPlayer);
    if (r.faulted || !r.bounded) g_halo3VolumeProbeEnabled.store(false, std::memory_order_release);
    g_halo3VolumeProbeStates[index].store(2, std::memory_order_release);
}

void Halo3PublishFreshRegion(uint64_t nowMs, uint32_t generation, uint64_t serial,
    PhysicalContactVec3 center, float bound, float worldScale, int32_t ignored,
    bool eligible,bool worldOwned=false)
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
            const uint64_t flags = (worldOwned ? 0ull : 9ull) | (0x7FFFull << 32);
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
    out.objectOnly.store(worldOwned,std::memory_order_relaxed);
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
    uint32_t nodeCount,bool worldOwned)
{
    if (!g_halo3FreshRegionEnabled.load(std::memory_order_acquire) ||
        !approved || !proposed || !nodeCount || nodeCount > 16)
        return false;
    const auto& pub = g_halo3FreshRegion;
    const uint32_t seq = pub.sequence.load(std::memory_order_acquire);
    if (seq & 1u) return false;
    if (pub.objectOnly.load(std::memory_order_relaxed) && !worldOwned) return false;
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
    Halo3LogWorldVolume();
    for (uint32_t index = 0; index < g_halo3VolumeProbeRecords.size(); ++index)
    {
        uint32_t ready = 2;
        if (!g_halo3VolumeProbeStates[index].compare_exchange_strong(ready, 3, std::memory_order_acquire)) continue;
        const auto& r = g_halo3VolumeProbeRecords[index];
        LOG("H3 volume PROBE: index=%u ms=%llu mode=%u surfaceType=%d radiusWu=%.6f start=(%.6f %.6f %.6f) motion=(%.6f %.6f %.6f) result=(%.6f %.6f %.6f) velocity=(%.6f %.6f %.6f) interior=%d gathered=%d features=%u/%u/%u collisions=%u clearanceM=%.6f bounded=%d faulted=%d costUs=%.1f",
            index, (unsigned long long)r.ms, r.mode, r.surfaceType, r.radius,
            r.start.x, r.start.y, r.start.z, r.motion.x, r.motion.y, r.motion.z,
            r.result.x, r.result.y, r.result.z, r.velocity.x, r.velocity.y, r.velocity.z,
            r.interior, r.gathered, r.featureCounts[0], r.featureCounts[1], r.featureCounts[2],
            r.collisions, r.clearance, r.bounded, r.faulted, r.microseconds);
        LOG("H3 whole volume PROBE: index=%u mode=%u type=%d built=%d seed=%d valid=%d blocked=%d exhausted=%d spheres=%u queries=%u progress=%.6f clearanceM=%.6f costUs=%.1f",
            index,r.mode,r.surfaceType,r.coverBuilt,r.coverSeed,r.coverValid,r.coverBlocked,r.coverExhausted,
            r.coverCount,r.coverQueries,r.coverProgress,r.coverClearance,r.coverMicroseconds);
    }
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
