// Current-pose world collision experiment. Native gathering stays on the
// simulation worker; rendering only queries pinned, self-contained features.
// ABI/math evidence: docs/HALO3-NATIVE-VOLUME-EVIDENCE.md.
#include "../common/halo3_world_recovery_replay.h"
struct Halo3WorldVolumeRequest
{
    uint32_t generation{}, reset{}, count{}, tag{};
    int32_t weapon{-1};
    uint64_t ms{}, serial{};
    std::array<BoneMatrix,16> nodes{};
};
struct Halo3WorldVolumeCache
{
    uint32_t generation{}, reset{}, count{}, tag{}, active{}, groups{};
    int32_t weapon{-1};
    uint64_t ms{}, epoch{}, shape{};
    float skin{}, regionRadius{}, bound{}, worldScale{};
    PhysicalContactVec3 center{};
    PhysicalContactVolumeCover cover{};
    std::array<BoneMatrix,16> nodes{};
    std::array<float,4> radii{};
    std::array<Halo3VolumeFeatures,4> features{};
    PhysicalContactVolumeRegions regions{};
    std::array<Halo3VolumeFeatures,PhysicalContactVolumeRegions::kMaximumRegions> regionFeatures{};
    PhysicalContactTransform seed{};
    bool seeded{},partitioned{};
};
struct Halo3WorldVolumePose
{
    uint32_t generation{}, reset{}, tag{}, active{};
    int32_t weapon{-1};
    uint64_t shape{}, ms{}, serial{};
    PhysicalContactTransform root{};
};
PhysicalContactSnapshot<Halo3WorldVolumeRequest> g_halo3WorldRequests;
PhysicalContactSnapshot<Halo3WorldVolumeCache> g_halo3WorldCaches;
PhysicalContactSnapshot<Halo3WorldVolumePose> g_halo3WorldPoses;
std::atomic<uint64_t> g_halo3WorldCacheSerial{0};
std::atomic<uint64_t> g_halo3WorldBuilds{0},g_halo3WorldSeeds{0},g_halo3WorldFrames{0};
std::atomic<uint64_t> g_halo3WorldBlocks{0},g_halo3WorldHolds{0},g_halo3WorldHidden{0};
std::atomic<uint64_t> g_halo3WorldUnknown{0},g_halo3WorldShapeRejects{0},g_halo3WorldFaults{0};
std::atomic<uint64_t> g_halo3WorldQueries{0},g_halo3WorldExhausted{0};
std::atomic<uint64_t> g_halo3WorldClockAdvances{0};
std::atomic<uint64_t> g_halo3WorldCoverReused{0},g_halo3WorldCoverChanged{0};
std::atomic<uint64_t> g_halo3WorldSeedRetests{0},g_halo3WorldSeedRetestClear{0},g_halo3WorldSeedRetestRejected{0};
std::atomic<uint64_t> g_halo3WorldRecoveryAttempts{0},g_halo3WorldRecoveryClear{0},g_halo3WorldRecoveryQueries{0};
std::atomic<uint64_t> g_halo3WorldRecoveryTicks{0},g_halo3WorldRecoveryMaxTicks{0};
Halo3WorldRecoveryReplay g_halo3WorldRecoveryReplay;
std::atomic<uint32_t> g_halo3WorldRecoveryReplayState{0}; // empty, writing, ready, consumed
std::atomic<bool> g_halo3WorldRecoveryReplayEnabled{false};
wchar_t g_halo3WorldRecoveryReplayPath[MAX_PATH]{};
std::atomic<uint64_t> g_halo3WorldCoverChecks{0},g_halo3WorldCoverTicks{0},g_halo3WorldCoverMaxTicks{0};
std::atomic<uint64_t> g_halo3WorldSolveMaxTicks{0},g_halo3WorldSolveTicks{0},g_halo3WorldSolveCount{0};
std::atomic<uint64_t> g_halo3WorldPartitionCalls{0},g_halo3WorldPartitionRegions{0};
std::atomic<uint64_t> g_halo3WorldPartitionCapacity{0},g_halo3WorldPartitionInvalid{0};
std::atomic<uint64_t> g_halo3WorldPartitionPlansFailed{0},g_halo3WorldPartitionMisses{0},g_halo3WorldPartitionCastInvalid{0};
std::atomic<uint64_t> g_halo3WorldPartitionTicks{0},g_halo3WorldPartitionMaxTicks{0};
std::array<std::atomic<uint32_t>,3> g_halo3WorldPartitionPeakCounts{};
struct Halo3WorldDraw
{
    uint32_t generation{},reset{},count{},tag{};
    int32_t weapon{-1};
    uint64_t serial{},ms{};
    int disposition{};
    std::array<BoneMatrix,16> nodes{},tracked{};
};
PhysicalContactSnapshot<Halo3WorldDraw> g_halo3WorldDraws;
struct Halo3WorldMeshAudit
{
    uint64_t ms{},serial{};
    uint32_t triangles[2]{},inside[2]{},crossings[2]{};
    PhysicalContactVec3 roots[2]{};
    float gapMeters{},edgeRemainderMeters[2]{};
    double microseconds{};
    int disposition{};
    bool valid{},faulted{};
};
// 256 observations at 2.5 s spacing cover the 300 s validation + 90 s
// recovery hold, including raw poses that never acquire solver ownership.
// Native queries remain diagnostic-only and never run from rendering.
std::array<Halo3WorldMeshAudit,256> g_halo3WorldMeshAudits{};
std::array<std::atomic<uint32_t>,256> g_halo3WorldMeshAuditStates{};
struct Halo3WorldGatherAudit
{
    uint64_t ms{};
    uint32_t reason{},validation{},group{},active{};
    uint16_t counts[3]{};
    bool gathered{};
    float radius{},expansion{},bound{};
    PhysicalContactVec3 center{};
};
std::array<Halo3WorldGatherAudit,20> g_halo3WorldGatherAudits{};
std::array<std::atomic<uint32_t>,20> g_halo3WorldGatherAuditStates{};
struct Halo3WorldHandoffObservation
{
    uint64_t ms{},serial{},age{};
    int32_t weapon{-1};
    int proposal{},final{};
    uint32_t proof{},category{};
    float gapMeters{};
    PhysicalContactVec3 tracked{},submitted{};
    Halo3WorldConstraintObservation before{},after{};
};
std::array<Halo3WorldHandoffObservation,96> g_halo3WorldHandoffRecords{};
std::array<std::atomic<uint32_t>,96> g_halo3WorldHandoffStates{};
std::array<std::atomic<uint64_t>,6> g_halo3WorldHandoffCounts{};
std::array<std::atomic<uint64_t>,6> g_halo3WorldHandoffNextMs{};
std::array<std::atomic<uint32_t>,6> g_halo3WorldHandoffReservations{};

void Halo3ObserveWorldHandoff(uint64_t nowMs,uint64_t serial,uint64_t originMs,
    int32_t weapon,int proposal,int final,uint32_t proof,const BoneMatrix& tracked,
    const BoneMatrix& submitted,const Halo3WorldConstraintObservation& before,
    const Halo3WorldConstraintObservation& after)
{
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        g_halo3WorldGatherOnly.load(std::memory_order_acquire)) return;
    const float scale=g_worldScale.load(std::memory_order_relaxed);
    const PhysicalContactVec3 a{tracked.translation[0],tracked.translation[1],tracked.translation[2]};
    const PhysicalContactVec3 b{submitted.translation[0],submitted.translation[1],submitted.translation[2]};
    if (!PhysicalContactFinite(a) || !PhysicalContactFinite(b) || !std::isfinite(scale) || scale<.05f || scale>2) return;
    const float gap=PhysicalContactLength(a-b)/scale;
    if (!std::isfinite(gap)) return;
    // Independent reservations preserve late failures after initial controls.
    // Category 0: early ownership lost; 1: visible over-leash; 2: hidden;
    // 3: remaining controls; 4: final cache newer than the frame timestamp;
    // 5: final cache has neither a clear seed nor a matching safe pose.
    // Failures retain priority over clock cases. Decisions are unchanged here.
    const bool newer=after.cacheMs>nowMs && after.cacheMs<=after.evaluationMs;
    const uint32_t category=proposal && !final ? 0u :
        (final!=2 && gap>.30f ? 1u : (newer ? 4u :
        (final==2 ? 2u : (after.reason==5 ? 5u : 3u))));
    g_halo3WorldHandoffCounts[category].fetch_add(1,std::memory_order_relaxed);
    const uint32_t limit=category<2 || category>=4 ? 16u : 4u;
    if (g_halo3WorldHandoffReservations[category].load(std::memory_order_relaxed)>=limit) return;
    uint64_t next=g_halo3WorldHandoffNextMs[category].load(std::memory_order_relaxed);
    if (nowMs<next || !g_halo3WorldHandoffNextMs[category].compare_exchange_strong(
            next,nowMs+(category==5 ? 1000 : 100),std::memory_order_relaxed)) return;
    const auto ordinal=g_halo3WorldHandoffReservations[category].fetch_add(1,std::memory_order_relaxed);
    if (ordinal>=limit) return;
    const auto index=category*16+ordinal;
    g_halo3WorldHandoffRecords[index]={nowMs,serial,nowMs>=originMs?nowMs-originMs:0,
        weapon,proposal,final,proof,category,gap,a,b,before,after};
    g_halo3WorldHandoffStates[index].store(2,std::memory_order_release);
}

// Simulation worker only; cold logging consumes immutable records. Reserve
// sixteen failure records independently of four success controls.
void Halo3ObserveWorldGather(const Halo3WorldVolumeCache& c,uint32_t group,
    uint32_t reason,uint32_t validation,bool gathered,const uint16_t* counts)
{
    if (!g_halo3WorldGatherOnly.load(std::memory_order_relaxed)) return;
    static unsigned successes=0,failures=0;
    if ((!reason && successes>=4) || (reason && failures>=16)) return;
    const unsigned index=reason ? 4+failures++ : successes++;
    auto& r=g_halo3WorldGatherAudits[index];
    r.ms=c.ms; r.reason=reason; r.validation=validation; r.group=group; r.active=c.active;
    r.gathered=gathered; r.radius=c.regionRadius; r.expansion=c.radii[group];
    r.bound=c.bound; r.center=c.center;
    if (counts) memcpy(r.counts,counts,sizeof(r.counts));
    g_halo3WorldGatherAuditStates[index].store(2,std::memory_order_release);
}

void Halo3PublishWorldDraw(const BoneMatrix* nodes,const BoneMatrix* tracked,uint32_t count,
    uint16_t tag,int32_t weapon,uint32_t generation,uint64_t serial,uint64_t ms,int disposition)
{
    if (!g_halo3WorldMeshAuditEnabled.load(std::memory_order_acquire) ||
        !g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        g_halo3WorldGatherOnly.load(std::memory_order_acquire) || !nodes || !tracked ||
        !count || count>16) return;
    auto write=g_halo3WorldDraws.write();
    if (!write) return;
    auto& d=write.get();
    d.generation=generation; d.reset=g_halo3WorldReset.load(std::memory_order_acquire);
    d.count=count; d.tag=tag; d.weapon=weapon; d.serial=serial; d.ms=ms; d.disposition=disposition;
    memcpy(d.nodes.data(),nodes,count*sizeof(BoneMatrix));
    memcpy(d.tracked.data(),tracked,count*sizeof(BoneMatrix));
    write.publish(serial);
}

PhysicalContactTransform Halo3WorldTransform(const BoneMatrix& b)
{
    return {{b.translation[0],b.translation[1],b.translation[2]},
        {b.rotation[0],b.rotation[1],b.rotation[2]},
        {b.rotation[3],b.rotation[4],b.rotation[5]},
        {b.rotation[6],b.rotation[7],b.rotation[8]},b.scale};
}

// Independent worker-side observations, not a permission used by the solver.
// All triangle vertices/centroids and both directions of every edge are tested.
// This samples the authored collision mesh at the submitted palette; it is not
// an exhaustive triangle/interior intersection proof or a rendered-mesh census.
void Halo3CheckWorldDrawMesh(const Halo3WorldDraw& d,const unsigned char* weaponData,
    int32_t ignored,float worldScale,Halo3WorldMeshAudit& record)
{
    static thread_local PhysicalContactCompoundShape shape;
    static thread_local PhysicalContactTriangleMesh mesh;
    LARGE_INTEGER begin{},end{},frequency{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&begin);
    __try
    {
        const uint32_t active=g_halo3ClearanceActiveMask ? *g_halo3ClearanceActiveMask : 0;
        if (!active || (active&0xFFFF0000u) || !g_halo3ClearancePoint || !g_halo3CollisionTestVector) return;
        for (unsigned pose=0;pose<2;++pose)
        {
            const auto* nodes=pose ? d.nodes.data() : d.tracked.data();
            const auto root=Halo3WorldTransform(nodes[0]);
            record.roots[pose]=root.position;
            if (!PhysicalContactVolumeRigid(root) ||
                !Halo3ContactVisibleCollisionShape(weaponData,nodes,d.count,root,shape,false,nullptr,&mesh) ||
                !PhysicalContactTriangleMeshValid(mesh)) return;
            record.triangles[pose]=mesh.triangleCount;
            for (uint16_t triangle=0;triangle<mesh.triangleCount;++triangle)
            {
                PhysicalContactVec3 vertices[4]{};
                for (unsigned v=0;v<3;++v)
                    vertices[v]=PhysicalContactTransformPoint(root,mesh.triangles[triangle].vertices[v]);
                vertices[3]=(vertices[0]+vertices[1]+vertices[2])*(1.f/3.f);
                for (unsigned v=0;v<4;++v)
                {
                    if (!PhysicalContactFinite(vertices[v])) return;
                    int32_t type=-1;
                    if (g_halo3ClearancePoint(9,&vertices[v].x,ignored,d.weapon,&type) && type>=0 && type<4)
                        ++record.inside[pose];
                }
                for (unsigned edge=0;edge<3;++edge) for (unsigned reverse=0;reverse<2;++reverse)
                {
                    const auto from=vertices[reverse ? (edge+1)%3 : edge];
                    const auto to=vertices[reverse ? edge : (edge+1)%3];
                    const auto motion=to-from;
                    if (PhysicalContactLengthSquared(motion)<1.e-10f) continue;
                    Halo3CollisionResult hit{}; hit.type=-1; hit.fraction=1;
                    if (g_halo3CollisionTestVector(9,false,&from.x,&motion.x,ignored,d.weapon,-1,&hit))
                    {
                        if (!std::isfinite(hit.fraction) || hit.fraction<0 || hit.fraction>1) return;
                        // Discard numerical endpoint touches; the cover's skin
                        // should leave the submitted mesh strictly outside.
                        if (hit.type>=0 && hit.type<4 && hit.fraction>1.e-4f && hit.fraction<.9999f)
                        {
                            ++record.crossings[pose];
                            record.edgeRemainderMeters[pose]=std::max(record.edgeRemainderMeters[pose],
                                PhysicalContactLength(motion)*(1-hit.fraction)/worldScale);
                        }
                    }
                }
            }
        }
        record.valid=active==*g_halo3ClearanceActiveMask &&
            d.generation==g_halo3RuntimeGeneration.load(std::memory_order_acquire) &&
            d.reset==g_halo3WorldReset.load(std::memory_order_acquire);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) { record.faulted=true; }
    QueryPerformanceCounter(&end);
    if (frequency.QuadPart>0) record.microseconds=(end.QuadPart-begin.QuadPart)*1.e6/frequency.QuadPart;
}

void Halo3AuditWorldDraw(uint64_t nowMs,uint32_t generation,int32_t weapon,int32_t ignored,
    const unsigned char* weaponData,float worldScale)
{
    // The paired 256-triangle sword audit measured up to 25.5 ms. Keep its
    // native queries and render-side diagnostic snapshots out of ordinary use.
    // Neither this record nor the audit enable flag grants solver ownership.
    if (!g_halo3WorldMeshAuditEnabled.load(std::memory_order_acquire) ||
        !g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        g_halo3WorldGatherOnly.load(std::memory_order_acquire) ||
        !std::isfinite(worldScale) || worldScale<.05f || worldScale>2) return;
    static uint64_t lastMs=0,lastSerial=0;
    static unsigned next=0;
    if (next>=g_halo3WorldMeshAudits.size() || nowMs<lastMs || nowMs-lastMs<2500) return;
    auto draw=g_halo3WorldDraws.read();
    if (!draw) return;
    const auto& d=draw.get();
    if (d.generation!=generation || d.weapon!=weapon || !d.serial || d.serial==lastSerial ||
        d.reset!=g_halo3WorldReset.load(std::memory_order_acquire) || nowMs<d.ms || nowMs-d.ms>50) return;
    const float gap=PhysicalContactLength(Halo3WorldTransform(d.nodes[0]).position-
        Halo3WorldTransform(d.tracked[0]).position)/worldScale;
    if (!std::isfinite(gap)) return;
    // A raw/unowned pose also has zero correction. Skipping consecutive zero
    // gaps would suppress precisely the samples needed to detect that failure.
    lastMs=nowMs; lastSerial=d.serial;
    const unsigned index=next++;
    auto& record=g_halo3WorldMeshAudits[index];
    record.ms=nowMs; record.serial=d.serial; record.gapMeters=gap; record.disposition=d.disposition;
    Halo3CheckWorldDrawMesh(d,weaponData,ignored,worldScale,record);
    g_halo3WorldMeshAuditStates[index].store(2,std::memory_order_release);
}

bool Halo3WorldNodesMatch(const Halo3WorldVolumeCache& c,const BoneMatrix* nodes,uint32_t count)
{
    if (!nodes || count!=c.count || !count || count>16) return false;
    const auto a=Halo3WorldTransform(c.nodes[0]),b=Halo3WorldTransform(nodes[0]);
    if (!PhysicalContactVolumeRigid(a) || !PhysicalContactVolumeRigid(b) || a.scale!=b.scale) return false;
    for (uint32_t i=0;i<count;++i)
    {
        // The strict local-pose comparison includes every node, even nodes not
        // used by a collision child. The explicit lever bound below accounts
        // for its tolerances rather than calling a near-equality exact.
        const auto oldNode=Halo3WorldTransform(c.nodes[i]),node=Halo3WorldTransform(nodes[i]);
        if (!PhysicalContactNodeRigidlyUnchanged(a,b,oldNode,node) ||
            !PhysicalContactVolumeRigid(oldNode) || !PhysicalContactVolumeRigid(node)) return false;
        const auto oldPosition=PhysicalContactInverseTransformPoint(a,oldNode.position);
        const float localNodeScale=oldNode.scale/a.scale;
        const float lever=(c.bound/a.scale+PhysicalContactLength(oldPosition))/localNodeScale;
        const auto delta=[&](PhysicalContactVec3 u,PhysicalContactVec3 v) {
            return PhysicalContactInverseTransformVector(a,u*oldNode.scale)-
                PhysicalContactInverseTransformVector(b,v*node.scale);
        };
        const float deformation=(PhysicalContactLength(oldPosition-
            PhysicalContactInverseTransformPoint(b,node.position))+lever*std::sqrt(
                PhysicalContactLengthSquared(delta(oldNode.forward,node.forward))+
                PhysicalContactLengthSquared(delta(oldNode.left,node.left))+
                PhysicalContactLengthSquared(delta(oldNode.up,node.up))))*a.scale;
        // Ten millimetres were reserved in each stored sphere; only half is
        // admitted here, retaining numerical slack around the analytic bound.
        if (!std::isfinite(deformation) || deformation>.005f*c.worldScale) return false;
    }
    return true;
}

bool Halo3WorldGather(Halo3WorldVolumeCache& c,int32_t ignored)
{
    // SEH is isolated from snapshot RAII. The canary follows the native
    // 0xC408-byte store before anything is copied into a published slot.
    alignas(16) unsigned char memory[0xC490];
    __try
    {
        c.active=*g_halo3ClearanceActiveMask;
        if (!c.active || (c.active&0xFFFF0000u))
        { Halo3ObserveWorldGather(c,0,1,0,false,nullptr); return false; }
        for (uint32_t group=0;group<c.groups;++group)
        {
            if (c.radii[group]<=0) continue;
            memset(memory,0xCD,sizeof(memory));
            const bool gathered=g_halo3ClearanceGather(9,&c.center.x,c.regionRadius,0,
                c.radii[group],ignored,-1,memory);
            uint16_t counts[3]{}; memcpy(counts,memory,sizeof(counts));
            uint32_t validation=0;
            if (!Halo3VolumeFeaturesValid(memory,sizeof(memory),&validation))
            { Halo3ObserveWorldGather(c,group,2,validation,gathered,counts); return false; }
            if (!gathered && (counts[0] || counts[1] || counts[2]))
            { Halo3ObserveWorldGather(c,group,3,0,gathered,counts); return false; }
            for (size_t i=0xC408;i<sizeof(memory);++i) if (memory[i]!=0xCD)
            { Halo3ObserveWorldGather(c,group,4,0,gathered,counts); return false; }
            Halo3ObserveWorldGather(c,group,0,0,gathered,counts);
            memcpy(c.features[group].bytes.data(),memory,0xC408);
        }
        if (c.active!=*g_halo3ClearanceActiveMask)
        { Halo3ObserveWorldGather(c,0,5,0,false,nullptr); return false; }
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { g_halo3WorldFaults.fetch_add(1,std::memory_order_relaxed);
      Halo3ObserveWorldGather(c,0,6,0,false,nullptr); return false; }
}

// One simulation-context gather per small region. Nothing is published unless
// every region is complete and the active structure is unchanged. No native
// gather, allocation or feature copying occurs in the renderer.
bool Halo3GatherWorldPartitions(Halo3WorldVolumeCache& c,int32_t ignored)
{
    alignas(16) unsigned char memory[0xC490];
    __try
    {
        c.active=*g_halo3ClearanceActiveMask;
        if (!c.active || (c.active&0xFFFF0000u) || !c.regions.count ||
            c.regions.count>c.regions.kMaximumRegions) return false;
        for (uint32_t index=0;index<c.regions.count;++index)
        {
            const auto& region=c.regions.regions[index];
            memset(memory,0xCD,sizeof(memory));
            const bool gathered=g_halo3ClearanceGather(9,&region.center.x,region.radius,0,
                region.expansion,ignored,-1,memory);
            g_halo3WorldPartitionRegions.fetch_add(1,std::memory_order_relaxed);
            uint16_t counts[3]{}; memcpy(counts,memory,sizeof(counts));
            for (unsigned category=0;category<3;++category)
            {
                uint32_t peak=g_halo3WorldPartitionPeakCounts[category].load(std::memory_order_relaxed);
                for (unsigned attempt=0;counts[category]>peak && attempt<3;++attempt)
                    if (g_halo3WorldPartitionPeakCounts[category].compare_exchange_weak(
                            peak,counts[category],std::memory_order_relaxed)) break;
            }
            uint32_t validation=0;
            if (!Halo3VolumeFeaturesValid(memory,sizeof(memory),&validation))
            {
                (validation>>16==2 ? g_halo3WorldPartitionCapacity : g_halo3WorldPartitionInvalid)
                    .fetch_add(1,std::memory_order_relaxed);
                return false;
            }
            if (!gathered && (counts[0] || counts[1] || counts[2]))
            { g_halo3WorldPartitionInvalid.fetch_add(1,std::memory_order_relaxed); return false; }
            for (size_t i=0xC408;i<sizeof(memory);++i) if (memory[i]!=0xCD)
            { g_halo3WorldPartitionInvalid.fetch_add(1,std::memory_order_relaxed); return false; }
            memcpy(c.regionFeatures[index].bytes.data(),memory,0xC408);
        }
        return c.active==*g_halo3ClearanceActiveMask;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { g_halo3WorldFaults.fetch_add(1,std::memory_order_relaxed); return false; }
}

// Cached expanded features prove boundary clearance separately. This native
// test supplies the solid-interior proof; it never authorizes a sphere alone.
bool Halo3NativeSeedPointOutside(PhysicalContactVec3 center,int32_t ignored,uint32_t expectedActive)
{
    if (!g_halo3ClearancePoint || !g_halo3ClearanceActiveMask || !expectedActive ||
        (expectedActive&0xFFFF0000u) || !PhysicalContactFinite(center)) return false;
    __try
    {
        if (*g_halo3ClearanceActiveMask!=expectedActive) return false;
        int32_t type=0;
        return !g_halo3ClearancePoint(9,&center.x,ignored,-1,&type) &&
            *g_halo3ClearanceActiveMask==expectedActive;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    { g_halo3WorldFaults.fetch_add(1,std::memory_order_relaxed); return false; }
}

void Halo3PublishWorldVolume(uint64_t nowMs,uint32_t generation,int32_t weapon,
    uint16_t tag,int32_t ignored,const PhysicalContactCompoundShape& shape,
    const BoneMatrix* nodes,uint32_t count,float worldScale)
{
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        !g_halo3ContactLateUpdateInstalled.load(std::memory_order_acquire) ||
        !nodes || !count || count>16 || !generation || weapon==-1 || tag==0xFFFFu ||
        !std::isfinite(worldScale) || worldScale<.05f || worldScale>2) return;
    auto request=g_halo3WorldRequests.read();
    if (!request) return;
    const auto& r=request.get();
    const uint32_t reset=g_halo3WorldReset.load(std::memory_order_acquire);
    if (r.generation!=generation || r.reset!=reset || r.weapon!=weapon || r.tag!=tag ||
        r.count!=count || nowMs<r.ms || nowMs-r.ms>50) return;
    auto write=g_halo3WorldCaches.write();
    if (!write) return;
    auto& c=write.get();
    c.seeded=false;
    c.partitioned=g_halo3WorldPartitions.load(std::memory_order_acquire);
    c.regions.count=0;
    c.generation=generation; c.reset=reset; c.weapon=weapon; c.tag=tag; c.count=count;
    c.worldScale=worldScale; c.skin=.005f*worldScale;
    c.ms=nowMs; c.epoch=g_halo3ClearanceEpoch.load(std::memory_order_acquire);
    const uint64_t version=g_halo3WorldCacheSerial.fetch_add(1,std::memory_order_relaxed)+1;
    LARGE_INTEGER coverBegin{},coverEnd{}; QueryPerformanceCounter(&coverBegin);
    const auto currentRoot=Halo3WorldTransform(nodes[0]);
    PhysicalContactVolumeCover incomingCover{};
    if (!PhysicalContactVolumeRigid(currentRoot) || shape.childCount>4 ||
        !PhysicalContactBuildVolumeCover(shape,.12f*worldScale/currentRoot.scale,incomingCover)) return;
    auto old=g_halo3WorldCaches.read();
    const bool compatible=old && old.get().generation==generation && old.get().reset==reset &&
        old.get().weapon==weapon && old.get().tag==tag && old.get().worldScale==worldScale &&
        Halo3WorldNodesMatch(old.get(),nodes,count) && Halo3WorldNodesMatch(old.get(),r.nodes.data(),count);
    const bool reuse=compatible && old.get().groups==shape.childCount &&
        PhysicalContactVolumeCoverContains(old.get().cover,incomingCover,.005f*worldScale/currentRoot.scale);
    QueryPerformanceCounter(&coverEnd);
    const auto coverTicks=static_cast<uint64_t>(std::max<LONGLONG>(0,coverEnd.QuadPart-coverBegin.QuadPart));
    g_halo3WorldCoverChecks.fetch_add(1,std::memory_order_relaxed);
    g_halo3WorldCoverTicks.fetch_add(coverTicks,std::memory_order_relaxed);
    auto coverPeak=g_halo3WorldCoverMaxTicks.load(std::memory_order_relaxed);
    for (unsigned attempt=0;coverTicks>coverPeak && attempt<3;++attempt)
        if (g_halo3WorldCoverMaxTicks.compare_exchange_weak(coverPeak,coverTicks,std::memory_order_relaxed)) break;
    if (compatible && !reuse) g_halo3WorldCoverChanged.fetch_add(1,std::memory_order_relaxed);
    if (reuse)
    {
        const auto& previous=old.get();
        c.cover=previous.cover; c.bound=previous.bound;
        c.radii=previous.radii; c.groups=previous.groups; c.shape=previous.shape;
        // The containment proof left 5 mm around today's incoming geometry.
        // Future node deformation must be measured from today's nodes, not
        // an older reference on the opposite side of that allowance.
        for (uint32_t i=0;i<count;++i) c.nodes[i]=nodes[i];
        g_halo3WorldCoverReused.fetch_add(1,std::memory_order_relaxed);
    }
    else
    {
        const auto root=currentRoot;
        c.cover=incomingCover;
        c.radii.fill(0); c.groups=shape.childCount; c.bound=0;
        for (uint32_t i=0;i<count;++i) c.nodes[i]=nodes[i];
        for (uint16_t i=0;i<c.cover.count;++i)
        {
            auto& sphere=c.cover.spheres[i];
            if (sphere.child>=c.groups) return;
            sphere.radius+=.01f*worldScale/root.scale;
            c.radii[sphere.child]=std::max(c.radii[sphere.child],sphere.radius);
        }
        for (uint16_t i=0;i<c.cover.count;++i)
        {
            auto& sphere=c.cover.spheres[i];
            sphere.radius=c.radii[sphere.child];
            c.bound=std::max(c.bound,(PhysicalContactLength(sphere.center)+sphere.radius)*root.scale);
        }
        for (uint32_t group=0;group<c.groups;++group)
            c.radii[group]=c.radii[group]*root.scale+c.skin;
        c.shape=version;
        if (!Halo3WorldNodesMatch(c,r.nodes.data(),count)) return;
    }
    if (!std::isfinite(c.bound) || c.bound<=0 || c.bound>2.5f*worldScale) return;
    const auto raw=Halo3WorldTransform(r.nodes[0]);
    c.center=raw.position;
    c.regionRadius=c.bound+.60f*worldScale+c.skin;
    auto safe=g_halo3WorldPoses.read();
    const bool sameSafe=safe && safe.get().generation==generation && safe.get().reset==reset &&
        safe.get().weapon==weapon && safe.get().tag==tag && safe.get().shape==c.shape;
    // Shape/reset identity is deliberately not inherited. A nearby same-weapon
    // transform can be re-proved using today's complete cover and native scene.
    // Bound recovery to twice the visual leash; farther history cannot make
    // native gathering grow with an arbitrary old controller displacement.
    const bool retestSafe=c.partitioned && !sameSafe && safe &&
        safe.get().generation==generation && safe.get().weapon==weapon && safe.get().tag==tag &&
        PhysicalContactVolumeRigid(safe.get().root) && safe.get().root.scale==raw.scale &&
        PhysicalContactLength(safe.get().root.position-raw.position)<=.60f*worldScale;
    bool gathered=false;
    if (c.partitioned)
    {
        LARGE_INTEGER begin{},end{}; QueryPerformanceCounter(&begin);
        // The renderer retains the last safe pose until a new raw seed is
        // independently clear. Cover its path even beyond the visual leash:
        // raw-only regions can omit that start while raw is inside a wall,
        // leaving every subsequent solve unable to advance or recover.
        // The existing region/count/query budgets still bound all work.
        const auto from=(sameSafe || retestSafe) ? safe.get().root : raw;
        const bool planned=PhysicalContactBuildVolumeRegions(c.cover,from,raw,c.skin,
            .40f*worldScale,.20f*worldScale,.08f*worldScale,c.regions);
        if (!planned) g_halo3WorldPartitionPlansFailed.fetch_add(1,std::memory_order_relaxed);
        else gathered=Halo3GatherWorldPartitions(c,ignored);
        QueryPerformanceCounter(&end);
        const auto ticks=static_cast<uint64_t>(std::max<LONGLONG>(0,end.QuadPart-begin.QuadPart));
        g_halo3WorldPartitionCalls.fetch_add(1,std::memory_order_relaxed);
        g_halo3WorldPartitionTicks.fetch_add(ticks,std::memory_order_relaxed);
        auto peak=g_halo3WorldPartitionMaxTicks.load(std::memory_order_relaxed);
        for (unsigned attempt=0;ticks>peak && attempt<3;++attempt)
            if (g_halo3WorldPartitionMaxTicks.compare_exchange_weak(peak,ticks,std::memory_order_relaxed)) break;
    }
    else gathered=Halo3WorldGather(c,ignored);
    if (!gathered)
    { g_halo3WorldUnknown.fetch_add(1,std::memory_order_relaxed); return; }
    // Observation mode deliberately stops before seed testing/publication:
    // neither a known-clear seed nor a permission can escape this probe.
    if (g_halo3WorldGatherOnly.load(std::memory_order_acquire))
    { g_halo3WorldBuilds.fetch_add(1,std::memory_order_relaxed); return; }
    const bool carry=sameSafe && safe.get().active==c.active;
    if (carry)
    {
        c.seed=safe.get().root;
        c.seeded=true;
    }
    if (retestSafe)
    {
        g_halo3WorldSeedRetests.fetch_add(1,std::memory_order_relaxed);
        const bool clear=PhysicalContactVolumeSeedClear(c.cover,safe.get().root,c.skin,
            [&](PhysicalContactVec3 center,float radius) {
                return Halo3NativeVolumeSeedClear(center,radius,ignored,9,c.active);
            });
        (clear ? g_halo3WorldSeedRetestClear : g_halo3WorldSeedRetestRejected)
            .fetch_add(1,std::memory_order_relaxed);
        if (clear) { c.seed=safe.get().root; c.seeded=true; }
        else
        {
            LARGE_INTEGER begin{},end{}; QueryPerformanceCounter(&begin);
            uint32_t queries=0;
            uint32_t empty=0;
            const bool capture=g_halo3WorldRecoveryReplayEnabled.load(std::memory_order_acquire) &&
                g_halo3WorldRecoveryReplayState.compare_exchange_strong(empty,1,std::memory_order_acquire);
            auto& replay=g_halo3WorldRecoveryReplay;
            if (capture)
            {
                replay.bytes=sizeof(replay);
                strncpy_s(replay.source,HALOMCCVR_BUILD_COMMIT,_TRUNCATE);
                replay.ms=nowMs; replay.epoch=c.epoch; replay.shape=c.shape;
                replay.generation=generation; replay.reset=reset; replay.active=c.active;
                replay.tag=tag; replay.weapon=weapon; replay.worldScale=worldScale; replay.skin=c.skin;
                replay.historical=safe.get().root; replay.raw=raw; replay.cover=c.cover; replay.regions=c.regions;
                replay.faultsBefore=g_halo3WorldFaults.load(std::memory_order_relaxed);
                for (uint32_t index=0;index<c.regions.count;++index) replay.features[index]=c.regionFeatures[index];
            }
            g_halo3WorldRecoveryAttempts.fetch_add(1,std::memory_order_relaxed);
            const bool recovered=PhysicalContactRecoverVolumeSeed(c.cover,safe.get().root,
                safe.get().root.position-raw.position,c.skin,.10f*worldScale,512,c.seed,queries,
                [&](PhysicalContactVec3 center,float radius) {
                    Halo3WorldRecoveryQuery* record=nullptr;
                    if (capture && queries && queries<=replay.queries.size())
                    {
                        record=&replay.queries[queries-1]; record->center=center; record->radius=radius;
                        record->region=-1; record->outcome=0;
                    }
                    for (uint32_t index=0;index<c.regions.count;++index)
                    {
                        if (!PhysicalContactVolumeRegionContains(c.regions.regions[index],center,center,radius)) continue;
                        if (record) { record->region=static_cast<int32_t>(index); record->outcome=1; }
                        if (!Halo3VolumePointOutsideFeatures(c.regionFeatures[index].bytes.data(),
                            Halo3VolumeFeatures::kBytes,&center.x)) return false;
                        const bool outside=Halo3NativeSeedPointOutside(center,ignored,c.active);
                        if (record) record->outcome=outside?3:2;
                        return outside;
                    }
                    return false;
                });
            if (capture)
            {
                replay.queryCount=queries; replay.recovered=recovered?1:0;
                replay.result=recovered?c.seed:safe.get().root;
                replay.sceneStable=(c.epoch==g_halo3ClearanceEpoch.load(std::memory_order_acquire) &&
                    reset==g_halo3WorldReset.load(std::memory_order_acquire))?1:0;
                replay.faultsAfter=g_halo3WorldFaults.load(std::memory_order_relaxed);
                g_halo3WorldRecoveryReplayState.store(2,std::memory_order_release);
            }
            if (recovered) { c.seeded=true; g_halo3WorldRecoveryClear.fetch_add(1,std::memory_order_relaxed); }
            g_halo3WorldRecoveryQueries.fetch_add(queries,std::memory_order_relaxed);
            QueryPerformanceCounter(&end);
            const auto ticks=static_cast<uint64_t>(std::max<LONGLONG>(0,end.QuadPart-begin.QuadPart));
            g_halo3WorldRecoveryTicks.fetch_add(ticks,std::memory_order_relaxed);
            auto peak=g_halo3WorldRecoveryMaxTicks.load(std::memory_order_relaxed);
            for (unsigned attempt=0;ticks>peak && attempt<3;++attempt)
                if (g_halo3WorldRecoveryMaxTicks.compare_exchange_weak(peak,ticks,std::memory_order_relaxed)) break;
        }
    }
    // Re-seed only at an independently clear raw pose. A stationary native
    // first-hit query is directional and cannot prove an inside-wall reset.
    if (!c.seeded || PhysicalContactLength(c.seed.position-raw.position)>.30f*worldScale)
    {
        const bool clear=PhysicalContactVolumeSeedClear(c.cover,raw,c.skin,
            [&](PhysicalContactVec3 center,float radius) {
                return Halo3NativeVolumeSeedClear(center,radius,ignored,9,c.active);
            });
        if (clear)
        { c.seed=raw; c.seeded=true; g_halo3WorldSeeds.fetch_add(1,std::memory_order_relaxed); }
    }
    if (c.epoch!=g_halo3ClearanceEpoch.load(std::memory_order_acquire) ||
        reset!=g_halo3WorldReset.load(std::memory_order_acquire)) return;
    if (write.publish(version)) g_halo3WorldBuilds.fetch_add(1,std::memory_order_relaxed);
}

bool Halo3WorldVolumeOwns(uint32_t generation,int32_t weapon,uint64_t nowMs)
{
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        g_halo3WorldGatherOnly.load(std::memory_order_acquire)) return false;
    auto cache=g_halo3WorldCaches.read();
    if (!cache) return false;
    const auto& c=cache.get();
    return c.seeded && c.generation==generation && c.weapon==weapon &&
        c.reset==g_halo3WorldReset.load(std::memory_order_acquire) &&
        nowMs>=c.ms && nowMs-c.ms<=20 && c.epoch==g_halo3ClearanceEpoch.load(std::memory_order_acquire);
}

// Returns 0 before ownership, 1 for a swept/held physical pose, 2 when the
// weapon must be hidden during an unproved or over-leash recovery. The caller
// retains the physical pose separately from its final hidden draw matrices.
int Halo3ConstrainWorldVolume(BoneMatrix* nodes,uint32_t count,uint16_t tag,int32_t weapon,
    uint32_t generation,uint64_t serial,uint64_t nowMs,const BoneMatrix* tracked,bool publishRequest,
    Halo3WorldConstraintObservation& observation)
{
    observation={}; observation.reason=1;
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        !g_config.physical_weapon_contact || !Halo3PhysicalContactRenderGuardOnFoot(generation) ||
        !tracked || !nodes || !count || count>16) return 0;
    const uint32_t reset=g_halo3WorldReset.load(std::memory_order_acquire);
    if (publishRequest)
    {
        auto write=g_halo3WorldRequests.write();
        if (write)
        {
            auto& r=write.get(); r.generation=generation; r.reset=reset; r.weapon=weapon;
            r.tag=tag; r.count=count; r.ms=nowMs; r.serial=serial;
            memcpy(r.nodes.data(),tracked,count*sizeof(BoneMatrix)); write.publish(serial);
        }
    }
    if (g_halo3WorldGatherOnly.load(std::memory_order_acquire)) { observation.reason=2; return 0; }
    auto cache=g_halo3WorldCaches.read();
    if (!cache) { observation.reason=3; return 0; }
    const auto& c=cache.get();
    // The worker may publish while the earlier palette/body path is running.
    // Pin first, then sample time: an earlier frame timestamp can classify
    // that already-published cache as future and hide a near-hand weapon.
    // Keep nowMs on the raw request and pose provenance; only cache freshness
    // uses the actual evaluation clock. No age or epoch allowance is widened.
    const uint64_t cacheNowMs=GetTickCount64();
    observation.evaluationMs=cacheNowMs;
    if (c.ms>nowMs && c.ms<=cacheNowMs)
        g_halo3WorldClockAdvances.fetch_add(1,std::memory_order_relaxed);
    observation.count=c.count; observation.reset=c.reset; observation.active=c.active;
    observation.resetReason=g_halo3ContactDebugLastResetReason.load(std::memory_order_relaxed);
    observation.cacheMs=c.ms; observation.epoch=c.epoch; observation.shape=c.shape;
    observation.seeded=c.seeded;
    if (c.generation!=generation || c.reset!=reset || c.weapon!=weapon || c.tag!=tag)
    { observation.reason=4; return 0; }
    auto safe=g_halo3WorldPoses.read();
    const bool haveSafe=safe && safe.get().generation==generation && safe.get().reset==reset &&
        safe.get().weapon==weapon && safe.get().tag==tag && safe.get().shape==c.shape &&
        safe.get().active==c.active;
    observation.safe=haveSafe;
    if (!c.seeded && !haveSafe) { observation.reason=5; return 0; }
    auto pose=haveSafe ? safe.get().root : c.seed;
    // A worker's independently clear recovery seed supersedes an over-leash
    // old pose. It never authorizes a reset at an untested raw hand position.
    if (c.seeded && haveSafe && PhysicalContactLength(pose.position-
            Halo3WorldTransform(tracked[0]).position)>.30f*c.worldScale)
        pose=c.seed;
    const bool matching=Halo3WorldNodesMatch(c,nodes,count);
    const bool fresh=matching && cacheNowMs>=c.ms && cacheNowMs-c.ms<=20 &&
        c.epoch==g_halo3ClearanceEpoch.load(std::memory_order_acquire);
    observation.matching=matching; observation.fresh=fresh;
    if (!matching) g_halo3WorldShapeRejects.fetch_add(1,std::memory_order_relaxed);
    bool solved=false;
    const auto lastClear=pose;
    if (fresh)
    {
        const auto cast=[&](PhysicalContactVec3 start,PhysicalContactVec3 motion,float radius) {
            if (c.partitioned)
            {
                for (uint32_t index=0;index<c.regions.count;++index)
                {
                    if (!PhysicalContactVolumeRegionContains(c.regions.regions[index],start,start+motion,radius)) continue;
                    const auto result=Halo3CastVolumeFeatures(c.regionFeatures[index].bytes.data(),start,motion);
                    if (!result.valid) g_halo3WorldPartitionCastInvalid.fetch_add(1,std::memory_order_relaxed);
                    return result;
                }
                g_halo3WorldPartitionMisses.fetch_add(1,std::memory_order_relaxed);
                return PhysicalContactVolumeCast{};
            }
            if (std::max(PhysicalContactLength(start-c.center),PhysicalContactLength(start+motion-c.center))+
                    radius>c.regionRadius) return PhysicalContactVolumeCast{};
            for (uint32_t group=0;group<c.groups;++group)
                if (radius==c.radii[group]) return Halo3CastVolumeFeatures(c.features[group].bytes.data(),start,motion);
            return PhysicalContactVolumeCast{};
        };
        LARGE_INTEGER begin{},end{};
        QueryPerformanceCounter(&begin);
        const auto result=PhysicalContactSlideVolume(c.cover,pose,Halo3WorldTransform(nodes[0]),
            c.skin,.30f*c.worldScale,192,cast);
        QueryPerformanceCounter(&end);
        const uint64_t elapsed=static_cast<uint64_t>(std::max<LONGLONG>(0,end.QuadPart-begin.QuadPart));
        g_halo3WorldSolveTicks.fetch_add(elapsed,std::memory_order_relaxed);
        g_halo3WorldSolveCount.fetch_add(1,std::memory_order_relaxed);
        uint64_t maximum=g_halo3WorldSolveMaxTicks.load(std::memory_order_relaxed);
        for (unsigned attempt=0;elapsed>maximum && attempt<3;++attempt)
            if (g_halo3WorldSolveMaxTicks.compare_exchange_weak(maximum,elapsed,std::memory_order_relaxed)) break;
        g_halo3WorldQueries.fetch_add(result.queries,std::memory_order_relaxed);
        if (result.valid)
        {
            pose=result.pose; solved=true;
            g_halo3WorldFrames.fetch_add(1,std::memory_order_relaxed);
            if (result.blocked) g_halo3WorldBlocks.fetch_add(1,std::memory_order_relaxed);
            if (result.exhausted) g_halo3WorldExhausted.fetch_add(1,std::memory_order_relaxed);
        }
        else g_halo3WorldUnknown.fetch_add(1,std::memory_order_relaxed);
    }
    if (c.epoch!=g_halo3ClearanceEpoch.load(std::memory_order_acquire))
    { pose=lastClear; solved=false; }
    if (reset!=g_halo3WorldReset.load(std::memory_order_acquire)) { observation.reason=6; return 2; }
    if (!solved) g_halo3WorldHolds.fetch_add(1,std::memory_order_relaxed);
    BoneMatrix desired=nodes[0],inverse{},delta{};
    desired.scale=pose.scale;
    const float basis[9]{pose.forward.x,pose.forward.y,pose.forward.z,pose.left.x,pose.left.y,pose.left.z,
        pose.up.x,pose.up.y,pose.up.z};
    memcpy(desired.rotation,basis,sizeof(basis)); memcpy(desired.translation,&pose.position,sizeof(pose.position));
    std::array<BoneMatrix,16> moved{};
    bool valid=InvertBoneMatrix(nodes[0],inverse) && ComposeBoneMatrices(desired,inverse,delta);
    for (uint32_t i=0;valid && i<count;++i) valid=ComposeBoneMatrices(delta,nodes[i],moved[i]);
    if (!valid) { observation.reason=7; return 2; }
    memcpy(nodes,moved.data(),count*sizeof(BoneMatrix));
    if (solved && reset==g_halo3WorldReset.load(std::memory_order_acquire))
    {
        auto write=g_halo3WorldPoses.write();
        if (write)
        {
            write.get()={generation,reset,tag,c.active,weapon,c.shape,nowMs,serial,pose};
            write.publish(serial);
        }
    }
    const bool hide=!matching || cacheNowMs<c.ms || cacheNowMs-c.ms>100 ||
        PhysicalContactLength(pose.position-Halo3WorldTransform(tracked[0]).position)>.30f*c.worldScale;
    if (hide) g_halo3WorldHidden.fetch_add(1,std::memory_order_relaxed);
    observation.reason=hide?9:8;
    return hide ? 2 : 1;
}

void Halo3LogWorldVolume()
{
    if (!g_halo3WorldVolumeEnabled.load() && !g_halo3WorldBuilds.load()) return;
    // This existing cold logger owns environment/path access and disk I/O.
    // The simulation hook only copies one bounded snapshot; rendering never
    // captures features, allocates, reads environment variables or writes files.
    static bool replayConfigured=false;
    if (!replayConfigured)
    {
        replayConfigured=true;
        const DWORD length=GetEnvironmentVariableW(L"HALOMCCVR_H3_WORLD_REPLAY_PATH",
            g_halo3WorldRecoveryReplayPath,MAX_PATH);
        const bool enabled=length>0 && length<MAX_PATH && g_halo3WorldMeshAuditEnabled.load();
        g_halo3WorldRecoveryReplayEnabled.store(enabled,std::memory_order_release);
        if (enabled) LOG("H3 world recovery replay: armed for first rejected historical seed; diagnostic-only snapshot, no clearance permission");
    }
    uint32_t ready=2;
    if (g_halo3WorldRecoveryReplayState.compare_exchange_strong(ready,3,std::memory_order_acquire))
    {
        const HANDLE file=CreateFileW(g_halo3WorldRecoveryReplayPath,GENERIC_WRITE,FILE_SHARE_READ,
            nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        DWORD written=0;
        const bool saved=file!=INVALID_HANDLE_VALUE && WriteFile(file,&g_halo3WorldRecoveryReplay,
            sizeof(g_halo3WorldRecoveryReplay),&written,nullptr) && written==sizeof(g_halo3WorldRecoveryReplay);
        const DWORD error=saved?0:GetLastError();
        if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);
        LOG("H3 world recovery replay: saved=%d bytes=%lu error=%lu queries=%u recovered=%u stable=%u path=%ls",
            saved?1:0,written,error,g_halo3WorldRecoveryReplay.queryCount,g_halo3WorldRecoveryReplay.recovered,
            g_halo3WorldRecoveryReplay.sceneStable,g_halo3WorldRecoveryReplayPath);
    }
    LOG("H3 world volume EXPERIMENT: enabled=%d caches=%llu seeds=%llu frames=%llu blocks=%llu holds=%llu hidden=%llu unknown=%llu shapeRejects=%llu queries=%llu exhausted=%llu faults=%llu",
        g_halo3WorldVolumeEnabled.load()?1:0,g_halo3WorldBuilds.load(),g_halo3WorldSeeds.load(),
        g_halo3WorldFrames.load(),g_halo3WorldBlocks.load(),g_halo3WorldHolds.load(),g_halo3WorldHidden.load(),
        g_halo3WorldUnknown.load(),g_halo3WorldShapeRejects.load(),g_halo3WorldQueries.load(),
        g_halo3WorldExhausted.load(),g_halo3WorldFaults.load());
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    LOG("H3 world seed clearance: mode=expanded-feature-membership nonemptyClear=%llu nonemptyRejected=%llu; outside-solid proof and native capacity guards retained",
        g_halo3SeedFeatureClear.load(),g_halo3SeedFeatureRejected.load());
    LOG("H3 world motion clock: futureSamplesSkipped=%llu; newer motion samples do not expire by unsigned age wrap",
        g_halo3ContactFutureMotionSkips.load());
    LOG("H3 world historical seed: tested=%llu clear=%llu rejected=%llu; full current cover and scene revalidation, 0.60m candidate bound",
        g_halo3WorldSeedRetests.load(),g_halo3WorldSeedRetestClear.load(),g_halo3WorldSeedRetestRejected.load());
    const auto recoveryAttempts=g_halo3WorldRecoveryAttempts.load();
    if (recoveryAttempts && frequency.QuadPart>0)
        LOG("H3 world seed depenetration: attempts=%llu clear=%llu queries=%llu meanUs=%.1f maxUs=%.1f; maxShift=0.10m queryBudget=512, cached boundary plus native interior proof",
            recoveryAttempts,g_halo3WorldRecoveryClear.load(),g_halo3WorldRecoveryQueries.load(),
            g_halo3WorldRecoveryTicks.load()*1.e6/frequency.QuadPart/recoveryAttempts,
            g_halo3WorldRecoveryMaxTicks.load()*1.e6/frequency.QuadPart);
    LOG("H3 world handoff counts: lost=%llu visibleOverLeashWithoutLoss=%llu hidden=%llu controls=%llu clockCases=%llu missingSeed=%llu",
        g_halo3WorldHandoffCounts[0].load(),g_halo3WorldHandoffCounts[1].load(),
        g_halo3WorldHandoffCounts[2].load(),g_halo3WorldHandoffCounts[3].load(),g_halo3WorldHandoffCounts[4].load(),
        g_halo3WorldHandoffCounts[5].load());
    LOG("H3 world cache clock: mode=after-pin newerThanFrame=%llu; freshness=20ms hold=100ms, raw pose timestamps unchanged",
        g_halo3WorldClockAdvances.load());
    LOG("H3 world cover reuse: contained=%llu geometryChanged=%llu; complete incoming spheres plus 5mm deformation reserve required",
        g_halo3WorldCoverReused.load(),g_halo3WorldCoverChanged.load());
    const auto coverChecks=g_halo3WorldCoverChecks.load();
    if (coverChecks && frequency.QuadPart>0)
        LOG("H3 world cover timing: checks=%llu meanUs=%.1f maxUs=%.1f",
            coverChecks,g_halo3WorldCoverTicks.load()*1.e6/frequency.QuadPart/coverChecks,
            g_halo3WorldCoverMaxTicks.load()*1.e6/frequency.QuadPart);
    for (uint32_t index=0;index<g_halo3WorldHandoffRecords.size();++index)
    {
        uint32_t ready=2;
        if (!g_halo3WorldHandoffStates[index].compare_exchange_strong(ready,3,std::memory_order_acquire)) continue;
        const auto& r=g_halo3WorldHandoffRecords[index];
        const auto& a=r.before; const auto& b=r.after;
        LOG("H3 world handoff: index=%u category=%u ms=%llu serial=%llu weapon=0x%08X decisions=%d/%d proof=%u originAgeMs=%llu gapM=%.5f reasons=%u/%u cacheMs=%llu/%llu evaluationMs=%llu/%llu epochs=%llu/%llu shapes=%llu/%llu resets=%u/%u active=%u/%u nodes=%u/%u seeded=%d/%d safe=%d/%d matching=%d/%d fresh=%d/%d resetReasons=%u/%u tracked=(%.6f %.6f %.6f) submitted=(%.6f %.6f %.6f)",
            index,r.category,r.ms,r.serial,static_cast<uint32_t>(r.weapon),r.proposal,r.final,r.proof,r.age,r.gapMeters,
            a.reason,b.reason,a.cacheMs,b.cacheMs,a.evaluationMs,b.evaluationMs,a.epoch,b.epoch,a.shape,b.shape,a.reset,b.reset,a.active,b.active,
            a.count,b.count,a.seeded?1:0,b.seeded?1:0,a.safe?1:0,b.safe?1:0,a.matching?1:0,b.matching?1:0,
            a.fresh?1:0,b.fresh?1:0,a.resetReason,b.resetReason,
            r.tracked.x,r.tracked.y,r.tracked.z,r.submitted.x,r.submitted.y,r.submitted.z);
    }
    const auto partitionCalls=g_halo3WorldPartitionCalls.load();
    if (partitionCalls && frequency.QuadPart>0)
        LOG("H3 world partitions: calls=%llu regions=%llu capacity=%llu invalid=%llu planFailures=%llu regionMisses=%llu castInvalid=%llu peaks=%u/%u/%u meanGatherUs=%.1f maxGatherUs=%.1f",
            partitionCalls,g_halo3WorldPartitionRegions.load(),g_halo3WorldPartitionCapacity.load(),
            g_halo3WorldPartitionInvalid.load(),g_halo3WorldPartitionPlansFailed.load(),
            g_halo3WorldPartitionMisses.load(),g_halo3WorldPartitionCastInvalid.load(),
            g_halo3WorldPartitionPeakCounts[0].load(),g_halo3WorldPartitionPeakCounts[1].load(),
            g_halo3WorldPartitionPeakCounts[2].load(),
            g_halo3WorldPartitionTicks.load()*1.e6/frequency.QuadPart/partitionCalls,
            g_halo3WorldPartitionMaxTicks.load()*1.e6/frequency.QuadPart);
    const auto count=g_halo3WorldSolveCount.load();
    if (frequency.QuadPart>0 && count)
        LOG("H3 world volume timing: solves=%llu meanUs=%.1f maxUs=%.1f",
            count,g_halo3WorldSolveTicks.load()*1.e6/frequency.QuadPart/count,
            g_halo3WorldSolveMaxTicks.load()*1.e6/frequency.QuadPart);
    for (uint32_t index=0;index<g_halo3WorldGatherAudits.size();++index)
    {
        uint32_t ready=2;
        if (!g_halo3WorldGatherAuditStates[index].compare_exchange_strong(ready,3,std::memory_order_acquire)) continue;
        const auto& r=g_halo3WorldGatherAudits[index];
        LOG("H3 world gather PROBE: index=%u ms=%llu reason=%u validation=0x%08X group=%u active=0x%X gathered=%d counts=%u/%u/%u radius=%.6f expansion=%.6f bound=%.6f center=(%.6f %.6f %.6f)",
            index,r.ms,r.reason,r.validation,r.group,r.active,r.gathered?1:0,
            static_cast<unsigned>(r.counts[0]),static_cast<unsigned>(r.counts[1]),static_cast<unsigned>(r.counts[2]),
            r.radius,r.expansion,r.bound,r.center.x,r.center.y,r.center.z);
    }
    for (uint32_t index=0;index<g_halo3WorldMeshAudits.size();++index)
    {
        uint32_t ready=2;
        if (!g_halo3WorldMeshAuditStates[index].compare_exchange_strong(ready,3,std::memory_order_acquire)) continue;
        const auto& r=g_halo3WorldMeshAudits[index];
        LOG("H3 world mesh AUDIT: index=%u ms=%llu serial=%llu draw=%d valid=%d fault=%d triangles=%u/%u inside=%u/%u crossings=%u/%u edgeRemainderM=%.5f/%.5f gapM=%.5f raw=(%.6f %.6f %.6f) submitted=(%.6f %.6f %.6f) costUs=%.1f",
            index,r.ms,r.serial,r.disposition,r.valid,r.faulted,r.triangles[0],r.triangles[1],
            r.inside[0],r.inside[1],r.crossings[0],r.crossings[1],r.edgeRemainderMeters[0],r.edgeRemainderMeters[1],
            r.gapMeters,r.roots[0].x,r.roots[0].y,r.roots[0].z,r.roots[1].x,r.roots[1].y,r.roots[1].z,r.microseconds);
    }
}
