#pragma once
#include "physical_contact_volume_sweep.h"

// A complete native feature query for one sphere radius within a bounded ball.
// Matching expansion is exact: geometry expanded for a smaller sphere cannot
// authorize a larger sphere. The whole chord plus sphere must fit the ball.
struct PhysicalContactVolumeRegion
{
    PhysicalContactVec3 center{};
    float radius=0,expansion=0;
};
struct PhysicalContactVolumeRegions
{
    static constexpr uint32_t kMaximumRegions=96;
    std::array<PhysicalContactVolumeRegion,kMaximumRegions> regions{};
    uint32_t count=0;
};

inline bool PhysicalContactVolumeRegionContains(const PhysicalContactVolumeRegion& region,
    PhysicalContactVec3 start,PhysicalContactVec3 end,float radius)
{
    if (!PhysicalContactFinite(region.center) || !PhysicalContactFinite(start) ||
        !PhysicalContactFinite(end) || !std::isfinite(region.radius) ||
        !std::isfinite(radius) || radius<=0 || radius!=region.expansion) return false;
    // A ball is convex. Enclosing both endpoint balls encloses the full swept
    // capsule, not merely two endpoint samples. Round the measured bound up.
    const float bound=std::nextafter(std::max(PhysicalContactLength(start-region.center),
        PhysicalContactLength(end-region.center))+radius,INFINITY);
    return bound<=region.radius;
}

inline bool PhysicalContactBuildVolumeRegions(const PhysicalContactVolumeCover& cover,
    const PhysicalContactTransform& from,const PhysicalContactTransform& to,float skin,
    float reserve,float maximumCenterStep,float mergeAllowance,PhysicalContactVolumeRegions& output,
    uint32_t maximumRegions=PhysicalContactVolumeRegions::kMaximumRegions)
{
    output.count=0;
    PhysicalContactVolumePath path{};
    if (!cover.count || cover.count>cover.kMaximumSpheres ||
        !PhysicalContactBuildVolumePath(from,to,path) || !std::isfinite(skin) || skin<=0 ||
        !std::isfinite(reserve) || reserve<0 || !std::isfinite(maximumCenterStep) || maximumCenterStep<=0 ||
        !std::isfinite(mergeAllowance) || mergeAllowance<0 || !maximumRegions ||
        maximumRegions>output.kMaximumRegions) return false;
    float lever=0;
    for (uint16_t i=0;i<cover.count;++i)
    {
        const auto& sphere=cover.spheres[i];
        if (!PhysicalContactFinite(sphere.center) || !std::isfinite(sphere.radius) || sphere.radius<=0) return false;
        lever=std::max(lever,PhysicalContactLength(sphere.center)*from.scale);
    }
    const float travel=PhysicalContactLength(to.position-from.position)+lever*path.angle;
    const float requestedSteps=std::ceil(travel/maximumCenterStep);
    const auto angularSteps=PhysicalContactVolumeAngularSteps(lever,path.angle,skin,64);
    if (!std::isfinite(requestedSteps) || requestedSteps>64 || !angularSteps) return false;
    const uint32_t steps=std::max(angularSteps,static_cast<uint32_t>(std::max(1.f,requestedSteps)));
    PhysicalContactVolumeRegions candidate{};
    for (uint32_t step=0;step<steps;++step)
    {
        const auto a=PhysicalContactVolumePathAt(path,static_cast<float>(step)/steps);
        const auto b=PhysicalContactVolumePathAt(path,static_cast<float>(step+1)/steps);
        for (uint16_t i=0;i<cover.count;++i)
        {
            const auto& sphere=cover.spheres[i];
            const auto start=PhysicalContactTransformPoint(a,sphere.center);
            const auto end=PhysicalContactTransformPoint(b,sphere.center);
            PhysicalContactVolumeRegion next{};
            next.center=(start+end)*.5f;
            next.expansion=sphere.radius*from.scale+skin;
            const float arc=PhysicalContactVolumeArcError(PhysicalContactLength(sphere.center)*from.scale,path.angle/steps);
            next.radius=std::nextafter(std::max(PhysicalContactLength(start-next.center),
                PhysicalContactLength(end-next.center))+next.expansion+arc+reserve,INFINITY);
            const float limit=next.expansion+reserve+maximumCenterStep*.5f+skin+mergeAllowance;
            if (!PhysicalContactFinite(next.center) || !std::isfinite(next.radius) || next.radius>limit) return false;
            bool merged=false;
            for (uint32_t index=0;index<candidate.count;++index)
            {
                auto& previous=candidate.regions[index];
                if (previous.expansion!=next.expansion) continue;
                const auto delta=next.center-previous.center;
                const float distance=PhysicalContactLength(delta);
                if (distance+next.radius<=previous.radius) { merged=true; break; }
                if (distance+previous.radius<=next.radius) { previous=next; merged=true; break; }
                if (!std::isfinite(distance) || distance<=0) return false;
                const float radius=(previous.radius+next.radius+distance)*.5f;
                const auto center=previous.center+delta*((radius-previous.radius)/distance);
                // Recompute about the stored float center, rounding outward.
                const float enclosing=std::nextafter(std::max(
                    PhysicalContactLength(center-previous.center)+previous.radius,
                    PhysicalContactLength(center-next.center)+next.radius),INFINITY);
                if (enclosing<=limit)
                { previous.center=center; previous.radius=enclosing; merged=true; break; }
            }
            if (!merged)
            {
                if (candidate.count==maximumRegions) return false;
                candidate.regions[candidate.count++]=next;
            }
        }
    }
    if (!candidate.count) return false;
    output=candidate;
    return true;
}
