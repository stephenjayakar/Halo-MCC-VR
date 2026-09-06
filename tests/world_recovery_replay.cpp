#include "../src/common/halo3_world_recovery_replay.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <cstring>
#include <cctype>

int main(int argc,char** argv)
{
    if (argc!=2) { std::fprintf(stderr,"Usage: halo3_world_recovery_replay capture.bin\n"); return 2; }
    auto r=std::make_unique<Halo3WorldRecoveryReplay>();
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if (!input || input.tellg()!=static_cast<std::streamoff>(sizeof(*r))) {
        std::fprintf(stderr,"Invalid or incomplete capture size\n"); return 2;
    }
    input.seekg(0); input.read(reinterpret_cast<char*>(r.get()),sizeof(*r));
    bool sourceValid=r->source[40]=='\0';
    for (unsigned i=0;i<40;++i) sourceValid=sourceValid && std::isxdigit(static_cast<unsigned char>(r->source[i]));
    if (!input || r->magic!=r->kMagic || r->version!=1 || r->bytes!=sizeof(*r) || !sourceValid ||
        !r->queryCount || r->queryCount>r->queries.size() || r->regions.count>r->regions.kMaximumRegions ||
        !r->regions.count || !r->cover.count || r->cover.count>r->cover.kMaximumSpheres ||
        !PhysicalContactVolumeRigid(r->historical) || !PhysicalContactVolumeRigid(r->raw) ||
        !std::isfinite(r->worldScale) || r->worldScale<.05f || r->worldScale>2 ||
        !std::isfinite(r->skin) || r->skin<=0 || r->skin>.1f || r->recovered>1 || r->sceneStable!=1 ||
        r->faultsAfter!=r->faultsBefore) {
        std::fprintf(stderr,"Invalid, unstable or faulted capture header\n"); return 2;
    }
    for (uint32_t i=0;i<r->regions.count;++i)
        if (!Halo3VolumeFeaturesValid(r->features[i].bytes.data(),Halo3VolumeFeatures::kBytes)) {
            std::fprintf(stderr,"Invalid native features in region %u\n",i); return 2;
        }
    uint32_t cursor=0,queries=0,outcomes[4]{};
    bool exact=true;
    auto output=r->historical;
    const bool recovered=PhysicalContactRecoverVolumeSeed(r->cover,r->historical,
        r->historical.position-r->raw.position,r->skin,.10f*r->worldScale,512,output,queries,
        [&](PhysicalContactVec3 center,float radius) {
            if (cursor>=r->queryCount) { exact=false; return false; }
            const auto& q=r->queries[cursor++];
            if (std::memcmp(&center,&q.center,sizeof(center)) || radius!=q.radius || q.outcome>3) {
                exact=false; return false;
            }
            ++outcomes[q.outcome];
            int32_t region=-1;
            for (uint32_t i=0;i<r->regions.count;++i)
                if (PhysicalContactVolumeRegionContains(r->regions.regions[i],center,center,radius)) {
                    region=static_cast<int32_t>(i); break;
                }
            if (region!=q.region) { exact=false; return false; }
            if (region<0) { exact=exact && q.outcome==0; return false; }
            const bool boundaryClear=Halo3VolumePointOutsideFeatures(r->features[region].bytes.data(),
                Halo3VolumeFeatures::kBytes,&center.x);
            if (!boundaryClear) { exact=exact && q.outcome==1; return false; }
            // Native solid-interior outcomes are recorded observations. This
            // executable cannot invent native answers for unrecorded points.
            exact=exact && (q.outcome==2 || q.outcome==3);
            return q.outcome==3;
        });
    exact=exact && cursor==r->queryCount && queries==r->queryCount && recovered==(r->recovered!=0);
    if (recovered) exact=exact && std::memcmp(&output,&r->result,sizeof(output))==0;
    std::printf("{\n  \"source\": \"%s\",\n  \"exact_replay\": %s,\n  \"recovered\": %s,\n"
        "  \"queries\": %u,\n  \"budget_exhausted\": %s,\n  \"cover_spheres\": %u,\n  \"regions\": %u,\n"
        "  \"missing_region\": %u,\n  \"expanded_boundary_rejects\": %u,\n  \"native_rejects\": %u,\n"
        "  \"clear_spheres\": %u,\n  \"headset_acceptance\": false\n}\n",r->source,exact?"true":"false",
        recovered?"true":"false",queries,queries==512?"true":"false",r->cover.count,r->regions.count,
        outcomes[0],outcomes[1],outcomes[2],outcomes[3]);
    return exact?0:1;
}
