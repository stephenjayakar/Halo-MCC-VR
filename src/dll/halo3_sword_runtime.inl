// Optional Halo 3 blade candidate. See HALO3-SWORD-CONTACT-EVIDENCE.md.
std::atomic<bool> g_halo3SwordExperiment{false};
struct Halo3SwordSelection
{
    std::atomic<uint32_t> sequence{0}, generation{0}, render{0xFFFFFFFFu}, definition{0xFFFFFFFFu};
    std::atomic<int32_t> weapon{-1};
    std::atomic<uint64_t> ms{0};
    std::atomic<bool> active{false};
    std::atomic<float> inverse[13]{};
};
Halo3SwordSelection g_halo3SwordSelection;
std::atomic<uint64_t> g_halo3SwordObservations{0}, g_halo3SwordPaired{0};
std::atomic<uint64_t> g_halo3SwordAppends{0}, g_halo3SwordStale{0}, g_halo3SwordRejected{0};

bool Halo3SwordInverseMatches(const BoneMatrix& b, float x)
{
    const float expected[13] = {1,1,0,0,0,1,0,0,0,1,x,0,0};
    float values[13]{};
    memcpy(values,&b,sizeof(b));
    for (int i=0;i<13;++i)
        if (!std::isfinite(values[i]) || std::fabs(values[i]-expected[i]) > 1.e-5f) return false;
    return true;
}

void Halo3ObserveSwordSelection(uint32_t tag, int32_t weapon, int regions, int nodes,
    int matrices, const uint16_t* meshes, const unsigned char* entry,
    const unsigned char* definition)
{
    if (!g_halo3SwordExperiment.load(std::memory_order_relaxed)) return;
    const uint32_t generation=g_halo3RuntimeGeneration.load(std::memory_order_acquire);
    if (!generation) return;
    // In the official 26-model first-person census only the sword has 2/2.
    // Confirm both loaded inverse matrices as well; never identify by tag index.
    if (regions != 2 || nodes != 2) return;
    auto* tagBase = g_halo3TagDataBase ? static_cast<const unsigned char*>(*g_halo3TagDataBase) : nullptr;
    const uint32_t nodeAddress = *reinterpret_cast<const uint32_t*>(definition+0x34);
    if (!tagBase || !nodeAddress) return;
    BoneMatrix inverse[2]{};
    for (int i=0;i<2;++i)
        memcpy(&inverse[i],tagBase+static_cast<size_t>(nodeAddress)*4+i*0x60+0x28,sizeof(BoneMatrix));
    if (!Halo3SwordInverseMatches(inverse[0],0) ||
        !Halo3SwordInverseMatches(inverse[1],-.11773931980133057f)) return;
    unsigned char* objectData = nullptr;
    uint8_t kind = 0xFF;
    if (!Halo3ContactObjectDataForHandle(weapon,objectData,&kind) || kind != 2) return;
    const uint32_t objectDefinition = *reinterpret_cast<const uint32_t*>(objectData);
    BoneMatrix drawn[Halo3VisibleWeaponPosePublication::kMaximumNodes]{};
    float basis[9]{},position[3]{},scale=1;
    uint64_t ms=0,serial=0;
    uint32_t count=0;
    uint16_t drawnTag=0xFFFF;
    int32_t drawnWeapon=-1;
    // Compare the exact submitted matrices, including deliberate draw hiding.
    // The consumer still builds the full blade from its unscaled physical
    // palette. Comparing against that physical palette here caused hiding to
    // remove blade geometry and permit handle-only seeds inside the wall.
    const bool paired = Halo3ReadWeaponPose(g_halo3SubmittedWeaponPose,basis,position,scale,ms,
        drawn,&count,&drawnTag,&drawnWeapon,&serial) && count==2 &&
        drawnTag==static_cast<uint16_t>(tag) && drawnWeapon==weapon &&
        !memcmp(drawn,entry+12,2*sizeof(BoneMatrix));
    g_halo3SwordObservations.fetch_add(1,std::memory_order_relaxed);
    if (paired) g_halo3SwordPaired.fetch_add(1,std::memory_order_relaxed);
    if (generation!=g_halo3RuntimeGeneration.load(std::memory_order_acquire)) return;
    auto& s=g_halo3SwordSelection;
    uint32_t before=s.sequence.load(std::memory_order_acquire);
    // One bounded publication attempt; a concurrent writer skips, never waits.
    if ((before&1) || !s.sequence.compare_exchange_strong(before,before+1,std::memory_order_acq_rel)) return;
    s.generation.store(generation,std::memory_order_relaxed);
    s.render.store(tag,std::memory_order_relaxed);
    s.definition.store(objectDefinition,std::memory_order_relaxed);
    s.weapon.store(weapon,std::memory_order_relaxed);
    float values[13]{}; memcpy(values,&inverse[1],sizeof(BoneMatrix));
    for (int i=0;i<13;++i) s.inverse[i].store(values[i],std::memory_order_relaxed);
    s.active.store(paired && matrices==2 && meshes[0]==0 && meshes[1]==1,std::memory_order_relaxed);
    s.ms.store(GetTickCount64(),std::memory_order_relaxed);
    s.sequence.store(before+2,std::memory_order_release);
}

void Halo3AppendLiveSwordGeometry(const unsigned char* objectData,const BoneMatrix* nodes,
    uint32_t nodeCount,const PhysicalContactTransform& root,
    PhysicalContactCompoundShape& compound,PhysicalContactTriangleMesh* mesh)
{
    if (!g_halo3SwordExperiment.load(std::memory_order_relaxed) || nodeCount!=2) return;
    auto& s=g_halo3SwordSelection;
    for (int attempt=0;attempt<3;++attempt)
    {
        const auto before=s.sequence.load(std::memory_order_acquire);
        if (before&1) continue;
        const auto generation=s.generation.load(std::memory_order_relaxed);
        const auto tag=s.render.load(std::memory_order_relaxed);
        const auto definition=s.definition.load(std::memory_order_relaxed);
        const auto weapon=s.weapon.load(std::memory_order_relaxed);
        const auto ms=s.ms.load(std::memory_order_relaxed);
        const bool active=s.active.load(std::memory_order_relaxed);
        float values[13]{};
        for (int i=0;i<13;++i) values[i]=s.inverse[i].load(std::memory_order_relaxed);
        if (s.sequence.load(std::memory_order_acquire)!=before) continue;
        if (generation!=g_halo3RuntimeGeneration.load(std::memory_order_acquire) ||
            weapon!=g_halo3ContactActiveWeaponHandle.load(std::memory_order_acquire) ||
            tag!=g_halo3ContactPreparedRenderDatum.load(std::memory_order_acquire) ||
            definition!=*reinterpret_cast<const uint32_t*>(objectData)) return;
        const uint64_t now=GetTickCount64();
        if (!ms || now<ms || now-ms>50)
        {
            g_halo3SwordStale.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        if (!active) return;
        BoneMatrix inverse{}; memcpy(&inverse,values,sizeof(inverse));
        if (Halo3AppendSwordBladeGeometry(Halo3ContactTransformFromBone(inverse),
                Halo3ContactTransformFromBone(nodes[1]),root,compound,mesh))
            g_halo3SwordAppends.fetch_add(1,std::memory_order_relaxed);
        else g_halo3SwordRejected.fetch_add(1,std::memory_order_relaxed);
        return;
    }
    g_halo3SwordStale.fetch_add(1,std::memory_order_relaxed);
}

void Halo3LogSwordGeometry()
{
    if (!g_halo3SwordExperiment.load(std::memory_order_relaxed)) return;
    LOG("H3 sword EXPERIMENT: observations=%llu paired=%llu appends=%llu stale=%llu rejected=%llu active=%d; missing/stale/unpaired selection retains handle-only contact",
        (unsigned long long)g_halo3SwordObservations.load(),(unsigned long long)g_halo3SwordPaired.load(),
        (unsigned long long)g_halo3SwordAppends.load(),(unsigned long long)g_halo3SwordStale.load(),
        (unsigned long long)g_halo3SwordRejected.load(),g_halo3SwordSelection.active.load());
}
