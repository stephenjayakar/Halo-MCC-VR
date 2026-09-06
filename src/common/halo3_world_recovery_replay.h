#pragma once
#include "physical_contact_volume_regions.h"
#include "halo3_volume_feature_logic.h"
#include <type_traits>

// Local, versioned diagnostic snapshot. It contains copied native geometry,
// never borrowed engine pointers. Capture and replay use this exact layout.
struct Halo3WorldRecoveryQuery
{
    PhysicalContactVec3 center{};
    float radius{};
    int32_t region{-1};
    uint32_t outcome{}; // 0 missing region, 1 expanded boundary, 2 native reject, 3 clear
};
struct Halo3WorldRecoveryReplay
{
    static constexpr uint64_t kMagic=0x31504C5257334848ull;
    uint64_t magic{kMagic};
    uint32_t version{1},bytes{};
    char source[48]{};
    uint64_t ms{},epoch{},shape{};
    uint32_t generation{},reset{},active{},tag{};
    int32_t weapon{-1};
    float worldScale{},skin{};
    uint32_t queryCount{},recovered{},sceneStable{};
    uint64_t faultsBefore{},faultsAfter{};
    PhysicalContactTransform historical{},raw{},result{};
    PhysicalContactVolumeCover cover{};
    PhysicalContactVolumeRegions regions{};
    std::array<Halo3VolumeFeatures,PhysicalContactVolumeRegions::kMaximumRegions> features{};
    std::array<Halo3WorldRecoveryQuery,512> queries{};
};
static_assert(std::is_trivially_copyable_v<Halo3WorldRecoveryReplay>);
static_assert(sizeof(Halo3WorldRecoveryReplay)<6*1024*1024);
