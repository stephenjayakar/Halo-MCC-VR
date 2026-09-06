// Offline replay of the production sword selection/append code. Engine reads
// and time are fakes; this does not validate retail pointers or hook ABI.
#include <atomic>
#include <cstring>
#include <iostream>
#include <limits>
#include "../src/common/physical_contact_logic.h"
#include "../src/common/halo3_sword_contact_logic.h"
#include "sword_runtime_poses.h"

struct BoneMatrix { float scale, rotation[9], translation[3]; };
static_assert(sizeof(BoneMatrix)==52);
struct Halo3VisibleWeaponPosePublication { static constexpr size_t kMaximumNodes=16; };
Halo3VisibleWeaponPosePublication g_halo3SubmittedWeaponPose;
std::atomic<uint32_t> g_halo3RuntimeGeneration{1};
std::atomic<int32_t> g_halo3ContactActiveWeaponHandle{static_cast<int32_t>(0xE46400B3u)};
std::atomic<uint32_t> g_halo3ContactPreparedRenderDatum{0xEF060D90u};
alignas(4) unsigned char tagData[320]{}, definitionData[56]{}, entryData[116]{};
uint32_t objectDefinition=0x12345678; // Deliberately synthetic; not an engine binding.
void* tagDataPointer=tagData;
void** g_halo3TagDataBase=&tagDataPointer;
BoneMatrix pose[2]{};
BoneMatrix submittedPose[2]{};
uint64_t fakeNow=1000;
uint32_t poseCount=2;
uint16_t poseTag=0x0D90;
int32_t poseWeapon=static_cast<int32_t>(0xE46400B3u);
uint8_t objectKind=2;
bool readPose=true, findObject=true, changeGenerationDuringRead=false;

uint64_t FakeTickCount() { return fakeNow; }
bool Halo3ContactObjectDataForHandle(int32_t handle,unsigned char*& data,uint8_t* kind)
{
    data=reinterpret_cast<unsigned char*>(&objectDefinition);
    *kind=objectKind;
    return findObject && handle==static_cast<int32_t>(0xE46400B3u);
}
bool Halo3ReadWeaponPose(Halo3VisibleWeaponPosePublication&,float*,float*,float&,
    uint64_t& ms,BoneMatrix* nodes,uint32_t* count,uint16_t* tag,int32_t* weapon,uint64_t* serial)
{
    ms=fakeNow; *serial=14202; *count=poseCount; *tag=poseTag; *weapon=poseWeapon;
    std::memcpy(nodes,submittedPose,sizeof(submittedPose));
    if (changeGenerationDuringRead) ++g_halo3RuntimeGeneration;
    return readPose;
}
PhysicalContactTransform Halo3ContactTransformFromBone(const BoneMatrix& b)
{
    PhysicalContactTransform t{};
    t.scale=b.scale;
    t.forward={b.rotation[0],b.rotation[1],b.rotation[2]};
    t.left={b.rotation[3],b.rotation[4],b.rotation[5]};
    t.up={b.rotation[6],b.rotation[7],b.rotation[8]};
    t.position={b.translation[0],b.translation[1],b.translation[2]};
    return t;
}
#define GetTickCount64 FakeTickCount
#define LOG(...) ((void)0)
#include "../src/dll/halo3_sword_runtime.inl"
#undef LOG
#undef GetTickCount64

static unsigned checks=0, failures=0;
static void Check(bool result,const char* name)
{
    ++checks;
    if (!result) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
static void SetPose(unsigned index)
{
    std::memcpy(pose,kSwordRuntimePoses[index],sizeof(pose));
    std::memcpy(submittedPose,pose,sizeof(pose));
    std::memcpy(entryData+12,pose,sizeof(pose));
}
static void Reset()
{
    g_halo3SwordExperiment=true;
    g_halo3RuntimeGeneration=1;
    g_halo3ContactActiveWeaponHandle=static_cast<int32_t>(0xE46400B3u);
    g_halo3ContactPreparedRenderDatum=0xEF060D90u;
    g_halo3SwordSelection.sequence=0; g_halo3SwordSelection.active=false;
    g_halo3SwordSelection.ms=0;
    g_halo3SwordObservations=0; g_halo3SwordPaired=0; g_halo3SwordAppends=0;
    g_halo3SwordStale=0; g_halo3SwordRejected=0;
    fakeNow=1000; poseCount=2; poseTag=0x0D90;
    poseWeapon=static_cast<int32_t>(0xE46400B3u);
    objectKind=2; objectDefinition=0x12345678;
    readPose=findObject=true; changeGenerationDuringRead=false;
    tagDataPointer=tagData;
    const uint32_t address=16;
    std::memcpy(definitionData+0x34,&address,sizeof(address));
    const float inverse[13]={1,1,0,0,0,1,0,0,0,1,0,0,0};
    std::memcpy(tagData+64+0x28,inverse,sizeof(inverse));
    std::memcpy(tagData+64+0x60+0x28,inverse,sizeof(inverse));
    const float childX=-.11773931980133057f;
    std::memcpy(tagData+64+0x60+0x28+40,&childX,sizeof(childX));
    SetPose(0);
}
static void Observe(int matrices=2,uint16_t first=0,uint16_t second=1,int regions=2,int nodes=2)
{
    const uint16_t meshes[2]={first,second};
    Halo3ObserveSwordSelection(0xEF060D90u,static_cast<int32_t>(0xE46400B3u),
        regions,nodes,matrices,meshes,entryData,definitionData);
}
static void Append(bool expected,const char* name,uint32_t count=2,bool exactMesh=true)
{
    // A synthetic valid base shape acts as a sentinel for handle preservation.
    static PhysicalContactCompoundShape compound;
    static PhysicalContactTriangleMesh mesh;
    compound={}; mesh={};
    compound.childCount=1;
    auto& child=compound.children[0];
    child.vertexCount=3;
    child.vertices[0]={0,0,0}; child.vertices[1]={.01f,0,0}; child.vertices[2]={0,.01f,0};
    mesh.triangleCount=mesh.groupCount=1;
    auto& triangle=mesh.triangles[0];
    std::copy_n(child.vertices.begin(),3,triangle.vertices.begin());
    triangle.centre={.005f,.005f,0}; triangle.halfExtents={.005f,.005f,0};
    triangle.boundRadius=.008f;
    auto& group=mesh.groups[0];
    group.triangleCount=1; group.centre=triangle.centre;
    group.halfExtents=triangle.halfExtents; group.boundRadius=triangle.boundRadius;
    const auto baseChild=child;
    const auto baseTriangle=triangle;
    const auto baseGroup=group;
    Halo3AppendLiveSwordGeometry(reinterpret_cast<unsigned char*>(&objectDefinition),pose,
        count,Halo3ContactTransformFromBone(pose[0]),compound,exactMesh ? &mesh : nullptr);
    Check(compound.childCount==(expected ? 3 : 1) &&
        mesh.triangleCount==(expected && exactMesh ? 245 : 1) &&
        mesh.groupCount==(expected && exactMesh ? 3 : 1),name);
    Check(!std::memcmp(&baseChild,&child,sizeof(child)) &&
        !std::memcmp(&baseTriangle,&triangle,sizeof(triangle)) &&
        !std::memcmp(&baseGroup,&group,sizeof(group)),"base shape preserved");
    Check(PhysicalContactCompoundValid(compound) && PhysicalContactTriangleMeshValid(mesh),
        "published geometry valid");
}
int main()
{
    Reset(); g_halo3SwordExperiment=false; Observe(); Append(false,"disabled experiment");
    Check(g_halo3SwordObservations==0,"disabled observations");
    for (unsigned record=0;record<2;++record)
    {
        Reset(); SetPose(record); Observe(); Append(true,"recorded paired sword palette");
        Check(g_halo3SwordPaired==1 && g_halo3SwordAppends==1,"paired/appended counters");
    }
    Reset(); Observe(); Append(true,"initial equipped sword");
    submittedPose[1].translation[0]+=.001f; Observe(); Append(false,"unpaired switch revokes blade");
    SetPose(1); Observe(); Append(true,"settled re-equip restores blade");
    Observe(1,0xFFFF,1); Append(false,"hidden first region revokes blade");
    Observe(); Append(true,"visible blade restored");
    Observe(2,0,0xFFFF); Append(false,"hidden second region");
    Observe(1); Append(false,"missing native matrix");
    Observe(); Append(true,"restored native selection");

    // Reproduce the runtime hiding boundary: actual draw scales collapse,
    // but collision retains the full physical palette and selected blades.
    Reset(); Observe(); Append(true,"visible before world hiding");
    for (auto& node:submittedPose) node.scale=.000001f;
    std::memcpy(entryData+12,submittedPose,sizeof(submittedPose));
    Observe(); Append(true,"hidden final draw retains full physical blades");
    Check(pose[0].scale>.01f && pose[1].scale>.01f,"physical scales were not collapsed");
    submittedPose[1].translation[0]+=.001f;
    Observe(); Append(false,"hidden draw still requires exact pairing");
    SetPose(1); Observe(); Append(true,"withdrawal returns to visible full blade");

    Reset(); Observe(); fakeNow+=50; Append(true,"50 ms freshness boundary");
    ++fakeNow; Append(false,"51 ms expires");
    Check(g_halo3SwordStale==1,"expired count");
    Reset(); Observe(); --fakeNow; Append(false,"future timestamp rejected");
    Reset(); fakeNow=0; Observe(); Append(false,"zero timestamp rejected");
    Reset(); Observe(); ++g_halo3RuntimeGeneration; Append(false,"map generation changed");
    Reset(); Observe(); g_halo3ContactPreparedRenderDatum=0xEF070D90u;
    Append(false,"render datum salt changed with identical low index");
    Reset(); Observe(); ++g_halo3ContactActiveWeaponHandle; Append(false,"active weapon changed");
    Reset(); Observe(); ++objectDefinition; Append(false,"object definition changed");
    Reset(); Observe(); Append(false,"consumer node count mismatch",1);
    Reset(); Observe(); Append(true,"hull-only consumer",2,false);

    Reset(); readPose=false; Observe(); Append(false,"pose read failed");
    Reset(); poseCount=1; Observe(); Append(false,"published node count mismatch");
    Reset(); ++poseWeapon; Observe(); Append(false,"published handle mismatch");
    Reset(); ++poseTag; Observe(); Append(false,"published render index mismatch");
    Reset(); objectKind=0; Observe(); Append(false,"non-weapon object rejected");
    Reset(); findObject=false; Observe(); Append(false,"missing object rejected");
    Reset(); Observe(2,0,1,1); Append(false,"wrong region count");
    Reset(); Observe(2,0,1,2,3); Append(false,"wrong authored node count");
    Reset(); tagDataPointer=nullptr; Observe(); Append(false,"missing tag base");
    Reset(); std::memset(definitionData+0x34,0,4); Observe(); Append(false,"missing node address");
    Reset(); const float wrongX=.5f;
    std::memcpy(tagData+64+0x60+0x28+40,&wrongX,4);
    Observe(); Append(false,"wrong child inverse fingerprint");
    Reset(); const float nan=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(tagData+64+0x28,&nan,4);
    Observe(); Append(false,"nonfinite root inverse fingerprint");
    Reset(); g_halo3RuntimeGeneration=0; Observe(); Append(false,"no map generation");
    Reset(); changeGenerationDuringRead=true; Observe(); Append(false,"generation changed during observation");
    Check(g_halo3SwordSelection.sequence==0,"generation change prevents publication");
    Reset(); g_halo3SwordSelection.sequence=1; Observe(); Append(false,"busy publisher never waits");
    Check(g_halo3SwordStale==1 && g_halo3SwordSelection.sequence==1,"bounded busy-reader return");
    Reset(); Observe(); pose[1].rotation[0]=nan; Append(false,"bad current palette retains handle");
    Check(g_halo3SwordRejected==1,"invalid geometry counter");
    std::cout << "Sword runtime replay: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
