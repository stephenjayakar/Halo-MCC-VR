#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

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
inline bool Halo3VolumeFeaturesValid(const void* memory,size_t bytes)
{
    if (!memory || bytes<Halo3VolumeFeatures::kBytes) return false;
    const auto* data=static_cast<const unsigned char*>(memory);
    uint16_t counts[3]{}; std::memcpy(counts,data,sizeof(counts));
    if (counts[0]>=256 || counts[1]>=256 || counts[2]>=256) return false;
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
        if (!finiteRange(p+0x14,4) || real(p+0x20)<0) return false;
    }
    for (unsigned i=0;i<counts[1];++i)
    {
        const auto* p=data+0x2408+i*0x30;
        if (!finiteRange(p+0x14,7) || real(p+0x2C)<0) return false;
    }
    for (unsigned i=0;i<counts[2];++i)
    {
        const auto* p=data+0x5408+i*0x70;
        int16_t projection=0; int32_t vertices=0;
        std::memcpy(&projection,p+0x28,sizeof(projection));
        std::memcpy(&vertices,p+0x2C,sizeof(vertices));
        if (projection<0 || projection>2 || p[0x2A]>1 || vertices<0 || vertices>8 ||
            !finiteRange(p+0x14,5) || !finiteRange(p+0x30,static_cast<unsigned>(vertices)*2)) return false;
    }
    return true;
}
