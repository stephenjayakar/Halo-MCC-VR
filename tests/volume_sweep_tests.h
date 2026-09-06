#pragma once
#include "physical_contact_volume_sweep.h"

static void TestPhysicalContactVolumeSweep()
{
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
