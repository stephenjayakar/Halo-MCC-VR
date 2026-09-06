#pragma once
#include "physical_contact_volume_sweep.h"
#include "physical_contact_volume_regions.h"
#include "halo3_volume_feature_logic.h"
#include "physical_contact_snapshot.h"
#include <thread>

static void TestContactSnapshotLifetime()
{
    struct Payload { uint64_t version=0; std::array<uint64_t,64> words{}; };
    PhysicalContactSnapshot<Payload> snapshots;
    Check(!snapshots.read(),"empty geometry snapshot does not masquerade as a clear world");
    const auto publish=[&](uint64_t version) {
        auto write=snapshots.write();
        if (!write) return false;
        write.get().version=version;
        write.get().words.fill(version);
        return write.publish(version);
    };
    Check(publish(1),"first immutable geometry snapshot publishes");
    auto first=snapshots.read();
    Check(bool(first) && first.version==1,"reader pins the exact published version");
    Check(publish(2),"producer can publish while an older snapshot remains pinned");
    auto second=snapshots.read();
    Check(publish(3),"three slots permit another publication with two old readers");
    Check(!publish(4) && first.get().version==1 && second.get().version==2,
        "slot exhaustion skips publication instead of overwriting a reader's geometry");
    // Retain old pins in this instance; exercise races in a separate exchange.
    PhysicalContactSnapshot<Payload> concurrent;
    std::atomic<uint64_t> next{0},writes{0},reads{0},errors{0};
    std::atomic<unsigned> writersLeft{2};
    const auto writer=[&] {
        for (unsigned i=0;i<4000;++i)
        {
            const uint64_t version=next.fetch_add(1)+1;
            auto claim=concurrent.write();
            if (!claim) continue;
            claim.get().version=version; claim.get().words.fill(version);
            if (claim.publish(version)) writes.fetch_add(1);
        }
        writersLeft.fetch_sub(1);
    };
    const auto reader=[&] {
        do
        {
            auto claim=concurrent.read();
            if (!claim) continue;
            const auto version=claim.version;
            std::this_thread::yield(); // Writers run while this geometry stays pinned.
            if (claim.get().version!=version) errors.fetch_add(1);
            for (auto word:claim.get().words) if (word!=version) errors.fetch_add(1);
            reads.fetch_add(1);
        } while (writersLeft.load()!=0);
    };
    std::thread readA(reader),readB(reader),writeA(writer),writeB(writer);
    writeA.join(); writeB.join(); readA.join(); readB.join();
    Check(writes.load()>0 && reads.load()>0 && errors.load()==0,
        "concurrent publishers never mutate pinned feature data or mismatch its version");
    auto latest=concurrent.read();
    Check(bool(latest),"a final published snapshot remains available after concurrent readers finish");
    auto old=concurrent.write();
    Check(bool(old) && !old.publish(1),"late old geometry cannot replace a newer published version");
}

static void TestPhysicalContactVolumeRegions()
{
    PhysicalContactVolumeCover cover{};
    for (unsigned side=0;side<2;++side) for (unsigned i=0;i<10;++i)
        cover.spheres[cover.count++]={{i*.12f,side ? -.08f : .08f,0},.075f,static_cast<uint16_t>(side)};
    for (float angle : {0.f,.2f,1.5f,3.14159265f})
    {
        PhysicalContactTransform from{},to{};
        from.position={-7,-8,-4}; to=from; to.position=to.position+PhysicalContactVec3{.2f,-.1f,.1f};
        const auto axis=PhysicalContactNormalize({1,2,-3});
        to.forward=PhysicalContactRotateAxisAngle(from.forward,axis,angle);
        to.left=PhysicalContactRotateAxisAngle(from.left,axis,angle);
        to.up=PhysicalContactRotateAxisAngle(from.up,axis,angle);
        PhysicalContactVolumeRegions regions{};
        const bool built=PhysicalContactBuildVolumeRegions(cover,from,to,.005f,.4f,.2f,.08f,regions);
        Check(built,"spatial regions cover a two-prong weapon's coupled translation and rotation");
        if (!built) continue;
        PhysicalContactVolumePath path{};
        Check(PhysicalContactBuildVolumePath(from,to,path),"region coverage reference path is rigid");
        bool allCovered=true;
        for (unsigned step=0;step<=400 && allCovered;++step)
        {
            const auto pose=PhysicalContactVolumePathAt(path,step/400.f);
            for (unsigned sphere=0;sphere<cover.count && allCovered;++sphere)
            {
                const auto& s=cover.spheres[sphere];
                const auto center=PhysicalContactTransformPoint(pose,s.center);
                bool covered=false;
                for (unsigned index=0;index<regions.count;++index)
                {
                    const auto& region=regions.regions[index];
                    // Independent point-on-arc check includes the full reserve
                    // ball, not just the center or the planning chords.
                    if (region.expansion==s.radius*pose.scale+.005f &&
                        PhysicalContactLength(center-region.center)+region.expansion+.4f<=region.radius+2.e-6f)
                    { covered=true; break; }
                }
                allCovered=covered;
            }
        }
        Check(allCovered,"every sampled rotational arc ball and its entire motion reserve fit a complete region");
        bool bounded=true;
        for (unsigned i=0;i<regions.count;++i)
            bounded=bounded && regions.regions[i].radius<.67f;
        Check(bounded,"region merging cannot grow back into the oversized whole-weapon query");
    }
    PhysicalContactVolumeRegions regions{};
    PhysicalContactTransform from{},to{}; to.position={0,.2f,0};
    Check(!PhysicalContactBuildVolumeRegions(cover,from,to,.005f,.4f,.2f,.08f,regions,1) && !regions.count,
        "region budget exhaustion never returns a partially covered weapon as complete");
    to.scale=2;
    Check(!PhysicalContactBuildVolumeRegions(cover,from,to,.005f,.4f,.2f,.08f,regions),
        "region planning cannot silently interpolate a changed weapon scale");
    const PhysicalContactVolumeRegion region{{0,0,0},.6f,.1f};
    Check(PhysicalContactVolumeRegionContains(region,{-.4f,0,0},{.4f,0,0},.1f),
        "a region admits a fully enclosed swept capsule");
    Check(!PhysicalContactVolumeRegionContains(region,{-.4f,0,0},{.55f,0,0},.1f),
        "an endpoint outside the contracted region cannot claim cached coverage");
    Check(!PhysicalContactVolumeRegionContains(region,{0,0,0},{.1f,0,0},.100001f),
        "a sphere cannot use geometry expanded for a different radius");
    auto invalid=region; invalid.radius=std::numeric_limits<float>::quiet_NaN();
    Check(!PhysicalContactVolumeRegionContains(invalid,{0,0,0},{0,0,0},.1f),
        "invalid cached bounds cannot authorize a zero-motion query");
}

static void TestPhysicalContactRecoveryRegions()
{
    // Raw is inside a wall, so an independent raw-seed test cannot replace
    // the retained safe position. This exceeds both the 30 cm visual leash
    // and the raw-only region's 40 cm motion reserve.
    PhysicalContactVolumeCover cover{};
    cover.count=1; cover.spheres[0]={{0,0,0},.075f,0};
    PhysicalContactTransform safe{},raw{}; raw.position={.6f,0,0};
    PhysicalContactVolumeRegions rawOnly{},recovery{};
    Check(PhysicalContactBuildVolumeRegions(cover,raw,raw,.005f,.4f,.2f,.08f,rawOnly) &&
        PhysicalContactBuildVolumeRegions(cover,safe,raw,.005f,.4f,.2f,.08f,recovery),
        "both recovery fixture region plans remain within their fixed budgets");
    const auto solve=[&](const PhysicalContactVolumeRegions& regions,
        const PhysicalContactTransform& from,const PhysicalContactTransform& to) {
        return PhysicalContactSlideVolume(cover,from,to,.005f,.3f,192,
            [&](PhysicalContactVec3 start,PhysicalContactVec3 motion,float radius) {
                bool covered=false;
                for (uint32_t i=0;i<regions.count;++i)
                    covered=covered || PhysicalContactVolumeRegionContains(
                        regions.regions[i],start,start+motion,radius);
                PhysicalContactVolumeCast hit{};
                if (!covered) return hit;
                hit.valid=true; hit.fraction=1;
                // Analytic wall occupies x >= .45; moving away is clear.
                if (motion.x>0 && start.x+motion.x+radius>=.45f) {
                    hit.hit=true; hit.normal={-1,0,0};
                    hit.fraction=std::clamp((.45f-radius-start.x)/motion.x,0.f,1.f);
                }
                return hit;
            });
    };
    const auto missing=solve(rawOnly,safe,raw);
    Check(!missing.valid && missing.queries>0,
        "raw-only recovery reproduces the unqueryable retained-safe path");
    const auto contact=solve(recovery,safe,raw);
    Check(contact.valid && contact.blocked && !contact.exhausted && !contact.leashExceeded &&
        contact.pose.position.x>.3f && contact.pose.position.x<.375f &&
        PhysicalContactLength(contact.pose.position-raw.position)<.3f,
        "covering the retained safe path reaches a clear wall contact within the visual leash");
    PhysicalContactVolumeRegions retreat{};
    Check(PhysicalContactBuildVolumeRegions(cover,contact.pose,safe,.005f,.4f,.2f,.08f,retreat),
        "retreat region plan covers the corrected contact pose");
    const auto released=solve(retreat,contact.pose,safe);
    Check(released.valid && !released.blocked && !released.exhausted &&
        PhysicalContactLength(released.pose.position-safe.position)<1.e-6f,
        "recovery contact releases immediately without replaying an old contact plane");
}

static void TestPhysicalContactChangedCoverSeed()
{
    PhysicalContactVolumeCover cover{};
    for (unsigned side=0;side<2;++side) for (unsigned i=0;i<10;++i)
        cover.spheres[cover.count++]={{i*.1f,side ? -.08f : .08f,0},.075f,static_cast<uint16_t>(side)};
    PhysicalContactTransform historical{},raw{};
    historical.position={-.15f,0,0}; raw.position={.15f,0,0};
    const auto outsideWall=[](PhysicalContactVec3 center,float radius) {
        return center.x+radius<1.f;
    };
    Check(!PhysicalContactVolumeSeedClear(cover,raw,.005f,outsideWall),
        "clear sword handle cannot approve a new blade penetrating the doorway");
    unsigned checked=0;
    Check(PhysicalContactVolumeSeedClear(cover,historical,.005f,
        [&](PhysicalContactVec3 center,float radius) { ++checked; return outsideWall(center,radius); }) &&
        checked==cover.count,"historical seed revalidation checks the entire new two-prong cover");
    auto grown=cover; grown.spheres[grown.count-1].center.x=1.2f;
    Check(!PhysicalContactVolumeSeedClear(grown,historical,.005f,outsideWall),
        "a formerly safe pose is rejected when the incoming blade extends into the wall");
    Check(!PhysicalContactVolumeSeedClear(cover,historical,.005f,
        [](PhysicalContactVec3 center,float radius) { return center.x+radius<.7f; }),
        "scene geometry moving into historical clearance prevents seed recovery");
    PhysicalContactVolumeRegions regions{};
    Check(PhysicalContactBuildVolumeRegions(cover,historical,raw,.005f,.4f,.2f,.08f,regions),
        "new blade recovery path fits bounded complete native regions");
    const auto sweep=[&](const PhysicalContactTransform& from,const PhysicalContactTransform& to) {
        return PhysicalContactSlideVolume(cover,from,to,.005f,.3f,192,
            [&](PhysicalContactVec3 start,PhysicalContactVec3 motion,float radius) {
                PhysicalContactVolumeCast hit{};
                for (uint32_t i=0;i<regions.count;++i)
                    hit.valid=hit.valid || PhysicalContactVolumeRegionContains(
                        regions.regions[i],start,start+motion,radius);
                if (!hit.valid) return hit;
                hit.fraction=1;
                if (motion.x>0 && start.x+motion.x+radius>=1.f) {
                    hit.hit=true; hit.normal={-1,0,0};
                    hit.fraction=std::clamp((1.f-radius-start.x)/motion.x,0.f,1.f);
                }
                return hit;
            });
    };
    const auto contact=sweep(historical,raw);
    Check(contact.valid && contact.blocked && !contact.leashExceeded && !contact.exhausted &&
        PhysicalContactVolumeSeedClear(cover,contact.pose,.0049f,outsideWall),
        "revalidated new blade reaches a clear doorway contact without losing ownership");
    const auto retreat=sweep(contact.pose,historical);
    Check(retreat.valid && !retreat.blocked && !retreat.exhausted &&
        PhysicalContactLength(retreat.pose.position-historical.position)<1.e-6f,
        "recovered blade follows the hand immediately on withdrawal");
    PhysicalContactVolumeCover empty{};
    Check(!PhysicalContactVolumeSeedClear(empty,historical,0,outsideWall),
        "missing geometry never revalidates a historical pose");
    auto invalid=cover; invalid.spheres[0].radius=-.001f;
    Check(!PhysicalContactVolumeSeedClear(invalid,historical,.005f,outsideWall),
        "query skin cannot mask an invalid negative cover radius");
}

static void TestPhysicalContactCoverReuse()
{
    auto shape=std::make_unique<PhysicalContactCompoundShape>();
    const auto box=[](PhysicalContactConvexShape& child,PhysicalContactVec3 half,PhysicalContactVec3 offset) {
        child={}; child.vertexCount=8;
        for (int i=0;i<8;++i) child.vertices[i]=offset+PhysicalContactVec3{
            (i&1)?half.x:-half.x,(i&2)?half.y:-half.y,(i&4)?half.z:-half.z};
    };
    shape->childCount=1;
    box(shape->children[0],{.05f,.03f,.04f},{});
    PhysicalContactVolumeCover handle{},incoming{};
    Check(PhysicalContactBuildVolumeCover(*shape,.12f,handle),"handle-only geometry builds a complete volume");
    const auto rawHandle=handle;
    for (uint16_t i=0;i<handle.count;++i) handle.spheres[i].radius+=.01f;
    Check(PhysicalContactVolumeCoverContains(handle,rawHandle,.005f),
        "unchanged geometry reuses its cache while retaining future animation reserve");
    shape->childCount=3;
    box(shape->children[1],{.45f,.01f,.02f},{.5f,.06f,0});
    box(shape->children[2],{.45f,.01f,.02f},{.5f,-.06f,0});
    Check(PhysicalContactBuildVolumeCover(*shape,.12f,incoming) &&
        !PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "a full two-prong blade cannot reuse handle-only coverage despite unchanged node transforms");
    shape->childCount=1;
    box(shape->children[0],{.55f,.03f,.04f},{});
    Check(PhysicalContactBuildVolumeCover(*shape,.12f,incoming) &&
        !PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "geometry growth within the same child also invalidates cached coverage");
    box(shape->children[0],{.05f,.03f,.04f},{}); shape->children[0].radius=.02f;
    Check(PhysicalContactBuildVolumeCover(*shape,.12f,incoming) &&
        !PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "increased authored collision padding cannot bypass reuse validation");
    incoming=rawHandle;
    for (uint16_t i=0;i<incoming.count;++i) incoming.spheres[i].center.x+=.001f;
    Check(PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "small numerical motion retains bounded coverage without forcing a new seed every sample");
    // Do not accumulate a new 5 mm allowance at each reuse. The outer sphere
    // stays fixed while the incoming geometry continues moving.
    for (uint16_t i=0;i<incoming.count;++i) incoming.spheres[i].center.x+=.010f;
    Check(!PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "repeated small geometry changes cannot drift beyond the original cached volume");
    incoming=rawHandle; incoming.spheres[0].child=1;
    Check(!PhysicalContactVolumeCoverContains(handle,incoming,.005f),
        "an overlapping unrelated child cannot supply the wrong expanded native feature group");
    incoming=rawHandle; incoming.spheres[0].radius=std::numeric_limits<float>::quiet_NaN();
    Check(!PhysicalContactVolumeCoverContains(handle,incoming,.005f),"invalid incoming geometry cannot reuse a cache");
    incoming=rawHandle; incoming.count=0;
    Check(!PhysicalContactVolumeCoverContains(handle,incoming,.005f),"an empty candidate is not a geometry proof");
    // Checking only sphere centres would incorrectly accept this case.
    incoming=rawHandle; incoming.spheres[0].radius=handle.spheres[0].radius+.001f;
    Check(!PhysicalContactVolumeCoverContains(handle,incoming,0),
        "equal centres do not establish containment of the complete incoming volume");
}

static void TestHalo3ExpandedFeatureClearance()
{
    auto f=std::make_unique<Halo3VolumeFeatures>();
    const auto word=[&](size_t at,uint16_t v) { std::memcpy(f->bytes.data()+at,&v,2); };
    const auto integer=[&](size_t at,int32_t v) { std::memcpy(f->bytes.data()+at,&v,4); };
    const auto real=[&](size_t at,float v) { std::memcpy(f->bytes.data()+at,&v,4); };
    const auto clear=[&](float x,float y,float z) {
        const float p[]{x,y,z}; return Halo3VolumePointOutsideFeatures(f->bytes.data(),f->bytes.size(),p);
    };
    Check(clear(0,0,0),"empty complete features are a point-clear negative control");
    word(0,1); real(8+0x20,1);
    Check(!clear(0,0,0) && !clear(1,0,0) && !clear(.999f,0,0),
        "expanded sphere interiors and boundaries cannot seed a weapon");
    Check(clear(1.01f,0,0),"a nearby expanded sphere does not reject a separated point");
    *f={}; word(2,1); real(0x2408+0x28,2); real(0x2408+0x2C,.5f);
    Check(!clear(.49f,0,1) && !clear(.5f,0,1) && clear(.51f,0,1),
        "finite cylinder radial clearance distinguishes interior, tangent and free points");
    Check(clear(0,0,-.01f) && clear(0,0,2.01f) && !clear(0,0,0) && !clear(0,0,2),
        "finite cylinder includes end disks while points beyond it are separate");
    // Native vertex spheres complete the rounded cylinder ends.
    word(0,1); real(8+0x20,.5f);
    Check(!clear(0,0,-.25f) && clear(0,0,-.51f),"the full feature union retains end-cap overlap");
    *f={}; word(2,1); real(0x2408+0x2C,.5f);
    Check(!clear(5,5,5),"a degenerate cylinder stays unknown rather than clearing the pose");
    constexpr unsigned axes[6][3]{{2,1,0},{1,2,0},{0,2,1},{2,0,1},{1,0,2},{0,1,2}};
    constexpr float polygon[4][2]{{-1,-1},{1,-1},{1,1},{-1,1}};
    for (unsigned row=0;row<6;++row)
    {
        *f={}; word(4,1); word(0x5408+0x28,row/2); f->bytes[0x5408+0x2A]=row%2;
        integer(0x5408+0x2C,4); real(0x5408+0x14+axes[row][2]*4,1); real(0x5408+0x24,.2f);
        for (unsigned j=0;j<4;++j) for (unsigned k=0;k<2;++k) real(0x5408+0x30+j*8+k*4,polygon[j][k]);
        const auto prismClear=[&](float x,float y,float height) {
            float p[3]{}; p[axes[row][0]]=x; p[axes[row][1]]=y; p[axes[row][2]]=height;
            return clear(p[0],p[1],p[2]);
        };
        Check(!prismClear(0,0,.1f) && !prismClear(1,0,.1f) && !prismClear(1,1,.1f),
            "every native prism projection rejects interior, edge and corner starts");
        Check(prismClear(1.01f,0,.1f) && prismClear(0,1.01f,.1f) && prismClear(1.01f,1.01f,.1f),
            "every native prism projection permits separated edge and corner starts");
        Check(prismClear(0,0,-.01f) && prismClear(0,0,.21f) &&
            !prismClear(0,0,0) && !prismClear(0,0,.2f),"both prism slab faces retain conservative boundaries");
    }
    // The polygon test projects onto the base plane along its normal, not by
    // simply dropping an axis. For n=(.6,0,.8), z-thickness shifts projected x.
    real(0x5408+0x14,.6f); real(0x5408+0x1C,.8f);
    Check(!clear(.66f,0,-.37f),"tilted prism interior is measured from its projected base point");
    Check(clear(1.3f,0,-.85f),"tilted prism separates outside its projected polygon");
    real(0x5408+0x24,-1);
    Check(!clear(0,0,5),"negative prism thickness is not a clearance proof");
    *f={}; word(4,256);
    Check(!clear(0,0,0),"saturated feature storage cannot prove clearance");
    *f={};
    Check(!clear(std::numeric_limits<float>::quiet_NaN(),0,0),"nonfinite seed input is rejected");
}

static void TestPhysicalContactVolumeSweep()
{
    TestHalo3ExpandedFeatureClearance();
    TestPhysicalContactCoverReuse();
    TestPhysicalContactChangedCoverSeed();
    TestPhysicalContactRecoveryRegions();
    TestPhysicalContactVolumeRegions();
    TestContactSnapshotLifetime();
    auto features=std::make_unique<Halo3VolumeFeatures>();
    Check(Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "empty gathered feature snapshots remain valid negative controls");
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()-1),
        "truncated feature snapshots cannot be queried");
    const auto set16=[&](size_t offset,uint16_t value) { std::memcpy(features->bytes.data()+offset,&value,2); };
    const auto set32=[&](size_t offset,uint32_t value) { std::memcpy(features->bytes.data()+offset,&value,4); };
    set16(0,256);
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "a saturated native feature category cannot claim complete coverage");
    set16(0,1); set32(8+0x20,0x7FC00000);
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "nonfinite sphere radius cannot enter cached native math");
    *features={}; set16(4,1); set16(0x5408+0x28,2); features->bytes[0x5408+0x2A]=1;
    set32(0x5408+0x2C,8);
    Check(Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "maximum bounded prism polygon and projection indices are admitted");
    set32(0x5408+0x2C,9);
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "prism polygon overflow cannot read beyond its embedded points");
    set32(0x5408+0x2C,8); set16(0x5408+0x28,3);
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "prism projection index cannot escape the audited read-only axis table");
    set16(0x5408+0x28,2); features->bytes[0x5408+0x2A]=2;
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "prism projection orientation cannot select an unproved table row");
    features->bytes[0x5408+0x2A]=1; set32(0x5408+0x30+7*8+4,0x7F800000);
    Check(!Halo3VolumeFeaturesValid(features->bytes.data(),features->bytes.size()),
        "every referenced polygon coordinate must remain finite");
    const auto rotate = [](PhysicalContactTransform t, PhysicalContactVec3 axis, float angle) {
        t.forward=PhysicalContactRotateAxisAngle(t.forward,axis,angle);
        t.left=PhysicalContactRotateAxisAngle(t.left,axis,angle);
        t.up=PhysicalContactRotateAxisAngle(t.up,axis,angle);
        return t;
    };
    for (auto axis : {PhysicalContactVec3{1,0,0},PhysicalContactVec3{0,1,0},
                     PhysicalContactVec3{0,0,1},PhysicalContactNormalize(PhysicalContactVec3{1,2,3})})
    for (float angle : {.0001f,.2f,1.5f,3.13f})
    {
        PhysicalContactTransform from{};
        from=rotate(from,PhysicalContactNormalize({3,-1,2}),.87f);
        from.position={3,-4,5}; from.scale=1.5f;
        auto to=rotate(from,axis,angle); to.position={-2,7,8};
        PhysicalContactVolumePath path{};
        Check(PhysicalContactBuildVolumePath(from,to,path),"rigid volume path accepts arbitrary relative rotation");
        bool sameArc=true;
        for (int i=0;i<=20;++i)
        {
            const float t=i/20.f;
            const auto actual=PhysicalContactVolumePathAt(path,t);
            const auto expected=rotate(from,axis,angle*t);
            sameArc = sameArc && PhysicalContactVolumeRigid(actual) &&
                PhysicalContactLength(actual.forward-expected.forward)<2.e-5f &&
                PhysicalContactLength(actual.left-expected.left)<2.e-5f &&
                PhysicalContactLength(actual.up-expected.up)<2.e-5f &&
                PhysicalContactLength(actual.position-(from.position+(to.position-from.position)*t))<2.e-5f;
        }
        Check(sameArc,"every volume path sample follows the analytic rigid rotation with unchanged scale");
    }
    for (auto axis : {PhysicalContactVec3{1,0,0},PhysicalContactVec3{0,1,0},PhysicalContactVec3{0,0,1}})
    {
        PhysicalContactTransform from{};
        const auto to=rotate(from,axis,3.14159265f);
        PhysicalContactVolumePath path{};
        Check(PhysicalContactBuildVolumePath(from,to,path) && std::abs(path.angle-3.14159265f)<1.e-5f &&
            PhysicalContactVolumeRigid(PhysicalContactVolumePathAt(path,.5f)),
            "half-turn extraction stays rigid without dividing by a vanishing quaternion scalar");
    }
    // Independent analytic half-space fixture, free region x >= sphere radius.
    // Starts inside are invalid: like retail, a forward cast cannot certify them.
    unsigned interiorCalls=0;
    const auto plane = [&](PhysicalContactVec3 start,PhysicalContactVec3 delta,float radius) {
        if (start.x < radius-1.e-6f) { ++interiorCalls; return PhysicalContactVolumeCast{}; }
        if (delta.x >= 0 || start.x+delta.x >= radius)
            return PhysicalContactVolumeCast{true,false,1,{}};
        return PhysicalContactVolumeCast{true,true,(radius-start.x)/delta.x,{1,0,0}};
    };
    PhysicalContactVolumeCover cover{};
    cover.count=2;
    cover.spheres[0]={{.3f,0,0},.05f,0};
    cover.spheres[1]={{-.3f,0,0},.05f,0};
    PhysicalContactTransform from{},to{};
    from.position.x=.8f; to.position.x=-.8f;
    const auto stopped=PhysicalContactSweepVolume(cover,from,to,.005f,128,plane);
    Check(stopped.valid && stopped.blocked && !stopped.exhausted &&
          stopped.pose.position.x>=.355f && stopped.pose.position.x<.358f,
        "earliest sphere contact stops the entire weapon at the independently calculated plane limit");
    Check(PhysicalContactLength(PhysicalContactTransformPoint(stopped.pose,cover.spheres[0].center)-
            PhysicalContactTransformPoint(stopped.pose,cover.spheres[1].center))>.59999f,
        "first-contact response preserves rigid separation between weapon volumes");
    to=stopped.pose; to.position.y=.7f;
    const auto tangent=PhysicalContactSweepVolume(cover,stopped.pose,to,.005f,128,plane);
    Check(tangent.valid && !tangent.blocked && tangent.progress==1 && tangent.pose.position.y==.7f,
        "a tangential move from the stopped pose retains all requested travel");
    to=stopped.pose; to.position.x=2;
    const auto retreat=PhysicalContactSweepVolume(cover,stopped.pose,to,.005f,128,plane);
    Check(retreat.valid && !retreat.blocked && retreat.progress==1 && retreat.pose.position.x==2,
        "moving away from a contact releases immediately without a settling spring or timer");
    Check(interiorCalls==0,"accepted native-query starts remain on the clear side of the full sphere");

    // Both endpoint spheres are clear. Only continuous rotation reveals that
    // the barrel passes through the wall between them.
    cover.count=1; cover.spheres[0]={{.6f,0,0},.02f,0};
    from={}; from.position.x=.35f;
    from=rotate(from,{0,0,1},2.0943951f);
    to=rotate(from,{0,0,1},2.0943951f);
    Check(PhysicalContactTransformPoint(from,cover.spheres[0].center).x>.025f &&
          PhysicalContactTransformPoint(to,cover.spheres[0].center).x>.025f,
        "rotation tunneling fixture has independently clear endpoints");
    const auto rotated=PhysicalContactSweepVolume(cover,from,to,.005f,128,plane);
    Check(rotated.valid && rotated.blocked && rotated.progress>0 && rotated.progress<.1f,
        "continuous rigid sweep stops a rotation that an endpoint-only test would miss");
    PhysicalContactVolumePath rotationPath{};
    Check(PhysicalContactBuildVolumePath(from,to,rotationPath),"rotation tunneling path is valid");
    bool keptClear=true;
    for (int i=0;i<=100;++i)
        keptClear=keptClear && PhysicalContactTransformPoint(
            PhysicalContactVolumePathAt(rotationPath,rotated.progress*i/100.f),cover.spheres[0].center).x>=.02f;
    Check(keptClear && interiorCalls==0,"the full accepted rotation arc stays outside the wall and all recasts start clear");

    const auto free=PhysicalContactSweepVolume(cover,from,to,.005f,128,
        [](PhysicalContactVec3,PhysicalContactVec3,float){ return PhysicalContactVolumeCast{true,false,1,{}}; });
    Check(free.valid && !free.blocked && !free.exhausted && free.progress==1 &&
          PhysicalContactLength(free.pose.forward-to.forward)<1.e-6f,
        "unobstructed rotation reaches the exact requested pose without smoothing");
    const auto budget=PhysicalContactSweepVolume(cover,from,to,.005f,1,
        [](PhysicalContactVec3,PhysicalContactVec3,float){ return PhysicalContactVolumeCast{true,false,1,{}}; });
    Check(budget.valid && budget.exhausted && budget.queries==1 && budget.progress<1,
        "query-budget exhaustion preserves only fully checked motion");
    const auto failed=PhysicalContactSweepVolume(cover,from,to,.005f,128,
        [](PhysicalContactVec3,PhysicalContactVec3,float){ return PhysicalContactVolumeCast{}; });
    Check(!failed.valid && failed.progress==0 && failed.queries==1,
        "native query failure cannot certify a proposed weapon pose");
    auto scaled=to; scaled.scale=2;
    PhysicalContactVolumePath badPath{};
    Check(!PhysicalContactBuildVolumePath(from,scaled,badPath),"scale change requires a new coverage proof");
    auto skewed=to; skewed.left=skewed.forward;
    Check(!PhysicalContactBuildVolumePath(from,skewed,badPath),"skewed palette cannot masquerade as a rigid sweep");

    // A hand target slightly inside the wall must still move tangentially.
    // Merely stopping at the first fraction would pin the gun at its old Y.
    cover.count=1; cover.spheres[0]={{},.04f,0};
    from={}; from.position={.2f,0,0}; to=from; to.position={-.05f,.7f,0};
    const auto slide=PhysicalContactSlideVolume(cover,from,to,.005f,.3f,128,plane);
    Check(slide.valid && slide.blocked && !slide.exhausted && !slide.leashExceeded &&
          std::abs(slide.pose.position.y-.7f)<1.e-6f &&
          slide.pose.position.x>=.045f && slide.pose.position.x<.046f,
        "an obstructed hand target slides the weapon along the wall without accumulating positional lag");
    to=slide.pose; to.position.x=.2f;
    const auto release=PhysicalContactSlideVolume(cover,slide.pose,to,.005f,.3f,128,plane);
    Check(release.valid && !release.blocked && release.pose.position.x==.2f && release.planes==0,
        "sliding does not retain a contact plane after the hand retreats");
    const auto corner=[&](PhysicalContactVec3 p,PhysicalContactVec3 d,float r) {
        auto first=plane(p,d,r);
        if (!first.valid) return first;
        if (p.y<r-1.e-6f) return PhysicalContactVolumeCast{};
        if (d.y<0 && p.y+d.y<r)
        {
            const float fraction=(r-p.y)/d.y;
            if (!first.hit || fraction<first.fraction)
                return PhysicalContactVolumeCast{true,true,fraction,{0,1,0}};
        }
        return first;
    };
    from.position={.2f,.2f,0}; to=from; to.position={-.05f,-.05f,.7f};
    const auto cornerSlide=PhysicalContactSlideVolume(cover,from,to,.005f,.3f,128,corner);
    Check(cornerSlide.valid && cornerSlide.blocked && !cornerSlide.exhausted &&
          !cornerSlide.leashExceeded && cornerSlide.planes>=2 &&
          cornerSlide.pose.position.x>=.045f && cornerSlide.pose.position.y>=.045f &&
          std::abs(cornerSlide.pose.position.z-.7f)<1.e-6f,
        "two independent wall contacts preserve sliding along the corner's free axis");
    to.position={-1.f,-1.f,0};
    const auto leash=PhysicalContactSlideVolume(cover,from,to,.005f,.3f,128,corner);
    Check(leash.valid && leash.blocked && leash.leashExceeded &&
          leash.pose.position.x>=.045f && leash.pose.position.y>=.045f,
        "unreachable hand targets request recovery while keeping the last swept pose outside the wall");
}
