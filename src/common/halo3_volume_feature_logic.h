#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cfloat>

// Pinned Halo 3 native gathered-feature layout. These are self-contained
// records, not engine object pointers. See HALO3-NATIVE-VOLUME-EVIDENCE.md.
struct Halo3VolumeFeatures
{
    static constexpr size_t kBytes=0xC408;
    alignas(16) std::array<unsigned char,kBytes> bytes{};
};

// Validate once before publishing an immutable snapshot. Besides the outer
// counts, the prism helper uses a projection-table index and polygon count to
// address its local arrays; those nested bounds must survive the copy too.
inline bool Halo3VolumeFeaturesValid(const void* memory,size_t bytes,uint32_t* failure=nullptr)
{
    // Optional diagnosis only; the admission rules are unchanged. High word:
    // 1 storage, 2 category capacity, 3 sphere, 4 cylinder, 5 prism.
    // Low word: record index (zero for storage/capacity).
    if (failure) *failure=0;
    const auto reject=[&](uint32_t reason,uint32_t index=0) {
        if (failure) *failure=(reason<<16)|index;
        return false;
    };
    if (!memory || bytes<Halo3VolumeFeatures::kBytes) return reject(1);
    const auto* data=static_cast<const unsigned char*>(memory);
    uint16_t counts[3]{}; std::memcpy(counts,data,sizeof(counts));
    if (counts[0]>=256 || counts[1]>=256 || counts[2]>=256) return reject(2);
    const auto real=[](const unsigned char* p) {
        float value=0; std::memcpy(&value,p,sizeof(value)); return value;
    };
    const auto finiteRange=[&](const unsigned char* p,unsigned count) {
        for (unsigned i=0;i<count;++i)
        {
            const float value=real(p+i*4);
            if (!std::isfinite(value) || std::abs(value)>1.e6f) return false;
        }
        return true;
    };
    for (unsigned i=0;i<counts[0];++i)
    {
        const auto* p=data+8+i*0x24;
        if (!finiteRange(p+0x14,4) || real(p+0x20)<0) return reject(3,i);
    }
    for (unsigned i=0;i<counts[1];++i)
    {
        const auto* p=data+0x2408+i*0x30;
        if (!finiteRange(p+0x14,7) || real(p+0x2C)<0) return reject(4,i);
    }
    for (unsigned i=0;i<counts[2];++i)
    {
        const auto* p=data+0x5408+i*0x70;
        int16_t projection=0; int32_t vertices=0;
        std::memcpy(&projection,p+0x28,sizeof(projection));
        std::memcpy(&vertices,p+0x2C,sizeof(vertices));
        if (projection<0 || projection>2 || p[0x2A]>1 || vertices<0 || vertices>8 ||
            !finiteRange(p+0x14,5) || !finiteRange(p+0x30,static_cast<unsigned>(vertices)*2)) return reject(5,i);
    }
    return true;
}

// Point membership in the already radius-expanded native feature union.
// Unlike first-hit, this has no motion-direction filter. The caller must also
// prove the point outside solid interiors and the gather complete/current.
// A false result means overlap OR uncertainty; it never authorizes recovery.
// Layout/math and projection table: HALO3-NATIVE-VOLUME-EVIDENCE.md.
inline bool Halo3VolumePointOutsideFeatures(const void* memory,size_t bytes,const float* point)
{
    if (!point || !Halo3VolumeFeaturesValid(memory,bytes)) return false;
    double q[3]{};
    double magnitude=1;
    for (unsigned j=0;j<3;++j)
    {
        if (!std::isfinite(point[j]) || std::abs(point[j])>1.e6f) return false;
        q[j]=point[j]; magnitude=std::max(magnitude,std::abs(q[j]));
    }
    const auto* data=static_cast<const unsigned char*>(memory);
    uint16_t counts[3]{}; std::memcpy(counts,data,sizeof(counts));
    const auto real=[](const unsigned char* p) {
        float v=0; std::memcpy(&v,p,4); return static_cast<double>(v);
    };
    const auto tolerance=[&](const unsigned char* p,unsigned n) {
        double scale=magnitude;
        for (unsigned j=0;j<n;++j) scale=std::max(scale,std::abs(real(p+j*4)));
        return std::max(1.e-5,8.0*FLT_EPSILON*scale);
    };
    for (unsigned i=0;i<counts[0];++i)
    {
        const auto* p=data+8+i*0x24;
        const double radius=real(p+0x20)+tolerance(p+0x14,4);
        double distance2=0;
        for (unsigned j=0;j<3;++j) { const double d=q[j]-real(p+0x14+j*4); distance2+=d*d; }
        if (distance2<=radius*radius) return false;
    }
    for (unsigned i=0;i<counts[1];++i)
    {
        const auto* p=data+0x2408+i*0x30;
        double delta[3]{},axis[3]{},length2=0,dot=0;
        const double epsilon=tolerance(p+0x14,7);
        for (unsigned j=0;j<3;++j)
        {
            delta[j]=q[j]-real(p+0x14+j*4); axis[j]=real(p+0x20+j*4);
            length2+=axis[j]*axis[j]; dot+=delta[j]*axis[j];
        }
        if (length2<=epsilon*epsilon) return false;
        const double length=std::sqrt(length2), t=dot/length2;
        if (dot < -epsilon*length || dot > length2+epsilon*length) continue;
        double radial2=0;
        for (unsigned j=0;j<3;++j) { const double d=delta[j]-axis[j]*t; radial2+=d*d; }
        const double radius=real(p+0x2C)+epsilon;
        if (radial2<=radius*radius) return false;
    }
    constexpr unsigned projectionAxes[6][2]{{2,1},{1,2},{0,2},{2,0},{1,0},{0,1}};
    for (unsigned i=0;i<counts[2];++i)
    {
        const auto* p=data+0x5408+i*0x70;
        int16_t projection=0; int32_t vertices=0;
        std::memcpy(&projection,p+0x28,2); std::memcpy(&vertices,p+0x2C,4);
        double n[3]{},n2=0,distance=-real(p+0x20);
        for (unsigned j=0;j<3;++j) { n[j]=real(p+0x14+j*4); n2+=n[j]*n[j]; distance+=q[j]*n[j]; }
        const double thickness=real(p+0x24);
        if (std::abs(n2-1)>0.002 || thickness<0 || vertices<3) return false;
        const double epsilon=std::max(tolerance(p+0x14,5),tolerance(p+0x30,vertices*2));
        if (distance < -epsilon || distance > thickness+epsilon) continue;
        const auto& axes=projectionAxes[projection*2+p[0x2A]];
        const double x=q[axes[0]]-n[axes[0]]*distance;
        const double y=q[axes[1]]-n[axes[1]]*distance;
        bool outside=false;
        for (int j=0;j<vertices;++j)
        {
            const int k=(j+1)%vertices;
            const double ax=real(p+0x30+j*8),ay=real(p+0x34+j*8);
            const double dx=real(p+0x30+k*8)-ax,dy=real(p+0x34+k*8)-ay;
            const double edge=std::sqrt(dx*dx+dy*dy);
            if (edge<=epsilon) return false;
            // Native convex halfspaces use cross(edge, projected-point)>=0.
            if (dx*(y-ay)-dy*(x-ax) < -epsilon*edge) outside=true;
        }
        if (!outside) return false;
    }
    return true;
}
