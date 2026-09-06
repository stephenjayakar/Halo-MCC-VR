// Current-pose world collision experiment. Native gathering stays on the
// simulation worker; rendering only queries pinned, self-contained features.
// ABI/math evidence: docs/HALO3-NATIVE-VOLUME-EVIDENCE.md.
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
    PhysicalContactTransform seed{};
    bool seeded{};
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
std::atomic<uint64_t> g_halo3WorldSolveMaxTicks{0},g_halo3WorldSolveTicks{0},g_halo3WorldSolveCount{0};
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
std::array<Halo3WorldMeshAudit,32> g_halo3WorldMeshAudits{};
std::array<std::atomic<uint32_t>,32> g_halo3WorldMeshAuditStates{};

void Halo3PublishWorldDraw(const BoneMatrix* nodes,const BoneMatrix* tracked,uint32_t count,
    uint16_t tag,int32_t weapon,uint32_t generation,uint64_t serial,uint64_t ms,int disposition)
{
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) || !nodes || !tracked ||
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
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire) ||
        !std::isfinite(worldScale) || worldScale<.05f || worldScale>2) return;
    static uint64_t lastMs=0,lastSerial=0;
    static unsigned next=0,freeSamples=0,hiddenSamples=0;
    if (next>=g_halo3WorldMeshAudits.size() || nowMs<lastMs || nowMs-lastMs<250) return;
    auto draw=g_halo3WorldDraws.read();
    if (!draw) return;
    const auto& d=draw.get();
    if (d.generation!=generation || d.weapon!=weapon || !d.serial || d.serial==lastSerial ||
        d.reset!=g_halo3WorldReset.load(std::memory_order_acquire) || nowMs<d.ms || nowMs-d.ms>50) return;
    const float gap=PhysicalContactLength(Halo3WorldTransform(d.nodes[0]).position-
        Halo3WorldTransform(d.tracked[0]).position)/worldScale;
    if (!std::isfinite(gap) || (gap<.005f && freeSamples>=4) ||
        (d.disposition==2 && hiddenSamples>=4)) return;
    if (gap<.005f) ++freeSamples;
    if (d.disposition==2) ++hiddenSamples;
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
        if (!c.active || (c.active&0xFFFF0000u)) return false;
        for (uint32_t group=0;group<c.groups;++group)
        {
            if (c.radii[group]<=0) continue;
            memset(memory,0xCD,sizeof(memory));
            const bool gathered=g_halo3ClearanceGather(9,&c.center.x,c.regionRadius,0,
                c.radii[group],ignored,-1,memory);
            uint16_t counts[3]{}; memcpy(counts,memory,sizeof(counts));
            if (!Halo3VolumeFeaturesValid(memory,sizeof(memory)) ||
                (!gathered && (counts[0] || counts[1] || counts[2]))) return false;
            for (size_t i=0xC408;i<sizeof(memory);++i) if (memory[i]!=0xCD) return false;
            memcpy(c.features[group].bytes.data(),memory,0xC408);
        }
        return c.active==*g_halo3ClearanceActiveMask;
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
    c.generation=generation; c.reset=reset; c.weapon=weapon; c.tag=tag; c.count=count;
    c.worldScale=worldScale; c.skin=.005f*worldScale;
    c.ms=nowMs; c.epoch=g_halo3ClearanceEpoch.load(std::memory_order_acquire);
    const uint64_t version=g_halo3WorldCacheSerial.fetch_add(1,std::memory_order_relaxed)+1;
    auto old=g_halo3WorldCaches.read();
    const bool reuse=old && old.get().generation==generation && old.get().reset==reset &&
        old.get().weapon==weapon && old.get().tag==tag && old.get().worldScale==worldScale &&
        Halo3WorldNodesMatch(old.get(),nodes,count) && Halo3WorldNodesMatch(old.get(),r.nodes.data(),count);
    if (reuse)
    {
        const auto& previous=old.get();
        c.cover=previous.cover; c.nodes=previous.nodes; c.bound=previous.bound;
        c.radii=previous.radii; c.groups=previous.groups; c.shape=previous.shape;
    }
    else
    {
        const auto root=Halo3WorldTransform(nodes[0]);
        if (!PhysicalContactVolumeRigid(root) || shape.childCount>4 ||
            !PhysicalContactBuildVolumeCover(shape,.12f*worldScale/root.scale,c.cover)) return;
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
    if (!Halo3WorldGather(c,ignored))
    { g_halo3WorldUnknown.fetch_add(1,std::memory_order_relaxed); return; }
    auto safe=g_halo3WorldPoses.read();
    const bool carry=safe && safe.get().generation==generation && safe.get().reset==reset &&
        safe.get().weapon==weapon && safe.get().tag==tag && safe.get().shape==c.shape &&
        safe.get().active==c.active;
    if (carry)
    {
        c.seed=safe.get().root;
        c.seeded=true;
    }
    // Re-seed only at an independently clear raw pose. A stationary native
    // first-hit query is directional and cannot prove an inside-wall reset.
    if (!carry || PhysicalContactLength(c.seed.position-raw.position)>.30f*worldScale)
    {
        bool clear=true;
        for (uint16_t i=0;i<c.cover.count && clear;++i)
        {
            const auto& sphere=c.cover.spheres[i];
            clear=Halo3NativeVolumeSeedClear(PhysicalContactTransformPoint(raw,sphere.center),
                c.radii[sphere.child],ignored,9);
        }
        if (clear)
        { c.seed=raw; c.seeded=true; g_halo3WorldSeeds.fetch_add(1,std::memory_order_relaxed); }
    }
    if (c.epoch!=g_halo3ClearanceEpoch.load(std::memory_order_acquire) ||
        reset!=g_halo3WorldReset.load(std::memory_order_acquire)) return;
    if (write.publish(version)) g_halo3WorldBuilds.fetch_add(1,std::memory_order_relaxed);
}

bool Halo3WorldVolumeOwns(uint32_t generation,int32_t weapon,uint64_t nowMs)
{
    if (!g_halo3WorldVolumeEnabled.load(std::memory_order_acquire)) return false;
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
    uint32_t generation,uint64_t serial,uint64_t nowMs,const BoneMatrix* tracked,bool publishRequest)
{
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
    auto cache=g_halo3WorldCaches.read();
    if (!cache) return 0;
    const auto& c=cache.get();
    if (c.generation!=generation || c.reset!=reset || c.weapon!=weapon || c.tag!=tag) return 0;
    auto safe=g_halo3WorldPoses.read();
    const bool haveSafe=safe && safe.get().generation==generation && safe.get().reset==reset &&
        safe.get().weapon==weapon && safe.get().tag==tag && safe.get().shape==c.shape &&
        safe.get().active==c.active;
    if (!c.seeded && !haveSafe) return 0;
    auto pose=haveSafe ? safe.get().root : c.seed;
    // A worker's independently clear recovery seed supersedes an over-leash
    // old pose. It never authorizes a reset at an untested raw hand position.
    if (c.seeded && haveSafe && PhysicalContactLength(pose.position-
            Halo3WorldTransform(tracked[0]).position)>.30f*c.worldScale)
        pose=c.seed;
    const bool matching=Halo3WorldNodesMatch(c,nodes,count);
    const bool fresh=matching && nowMs>=c.ms && nowMs-c.ms<=20 &&
        c.epoch==g_halo3ClearanceEpoch.load(std::memory_order_acquire);
    if (!matching) g_halo3WorldShapeRejects.fetch_add(1,std::memory_order_relaxed);
    bool solved=false;
    const auto lastClear=pose;
    if (fresh)
    {
        const auto cast=[&](PhysicalContactVec3 start,PhysicalContactVec3 motion,float radius) {
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
    if (reset!=g_halo3WorldReset.load(std::memory_order_acquire)) return 2;
    if (!solved) g_halo3WorldHolds.fetch_add(1,std::memory_order_relaxed);
    BoneMatrix desired=nodes[0],inverse{},delta{};
    desired.scale=pose.scale;
    const float basis[9]{pose.forward.x,pose.forward.y,pose.forward.z,pose.left.x,pose.left.y,pose.left.z,
        pose.up.x,pose.up.y,pose.up.z};
    memcpy(desired.rotation,basis,sizeof(basis)); memcpy(desired.translation,&pose.position,sizeof(pose.position));
    std::array<BoneMatrix,16> moved{};
    bool valid=InvertBoneMatrix(nodes[0],inverse) && ComposeBoneMatrices(desired,inverse,delta);
    for (uint32_t i=0;valid && i<count;++i) valid=ComposeBoneMatrices(delta,nodes[i],moved[i]);
    if (!valid) return 2;
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
    const bool hide=!matching || nowMs<c.ms || nowMs-c.ms>100 ||
        PhysicalContactLength(pose.position-Halo3WorldTransform(tracked[0]).position)>.30f*c.worldScale;
    if (hide) g_halo3WorldHidden.fetch_add(1,std::memory_order_relaxed);
    return hide ? 2 : 1;
}

void Halo3LogWorldVolume()
{
    if (!g_halo3WorldVolumeEnabled.load() && !g_halo3WorldBuilds.load()) return;
    LOG("H3 world volume EXPERIMENT: enabled=%d caches=%llu seeds=%llu frames=%llu blocks=%llu holds=%llu hidden=%llu unknown=%llu shapeRejects=%llu queries=%llu exhausted=%llu faults=%llu",
        g_halo3WorldVolumeEnabled.load()?1:0,g_halo3WorldBuilds.load(),g_halo3WorldSeeds.load(),
        g_halo3WorldFrames.load(),g_halo3WorldBlocks.load(),g_halo3WorldHolds.load(),g_halo3WorldHidden.load(),
        g_halo3WorldUnknown.load(),g_halo3WorldShapeRejects.load(),g_halo3WorldQueries.load(),
        g_halo3WorldExhausted.load(),g_halo3WorldFaults.load());
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    const auto count=g_halo3WorldSolveCount.load();
    if (frequency.QuadPart>0 && count)
        LOG("H3 world volume timing: solves=%llu meanUs=%.1f maxUs=%.1f",
            count,g_halo3WorldSolveTicks.load()*1.e6/frequency.QuadPart/count,
            g_halo3WorldSolveMaxTicks.load()*1.e6/frequency.QuadPart);
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
