#pragma once

#include "Shaft.h"
#include "SurfaceCache.h"

namespace Thread::NiNode::Surface::Geometry
{
    struct ActorGeometry
    {
        explicit ActorGeometry(RE::Actor* a_actor);

        RE::NiPointer<RE::NiNode> head;
        RE::NiPointer<RE::NiAVObject> mouth;
        RE::NiPointer<RE::NiNode> pelvis;
        RE::NiPointer<RE::NiNode> lowerSpine;

        RE::NiPointer<RE::NiNode> leftHand;
        RE::NiPointer<RE::NiNode> rightHand;
        RE::NiPointer<RE::NiNode> leftThumb;
        RE::NiPointer<RE::NiNode> rightThumb;
        RE::NiPointer<RE::NiNode> leftFoot;
        RE::NiPointer<RE::NiNode> rightFoot;
        RE::NiPointer<RE::NiNode> leftToe;
        RE::NiPointer<RE::NiNode> rightToe;

        RE::NiPointer<RE::NiNode> deepVagina;
        RE::NiPointer<RE::NiNode> leftVagina;
        RE::NiPointer<RE::NiNode> rightVagina;
        RE::NiPointer<RE::NiNode> deepAnus;
        RE::NiPointer<RE::NiNode> leftAnus;
        RE::NiPointer<RE::NiNode> rightAnus;
        std::vector<Shaft> shafts;

        RE::NiPointer<RE::NiNode> animObjectA;
        RE::NiPointer<RE::NiNode> animObjectB;
        RE::NiPointer<RE::NiNode> animObjectLeft;
        RE::NiPointer<RE::NiNode> animObjectRight;

        std::optional<GeometryMath::Segment> GetVaginalSegment() const;
        std::optional<GeometryMath::Segment> GetAnalSegment() const;
        std::optional<OpeningShape> GetMouthOpening() const;
        std::optional<OpeningShape> GetVaginalOpening();
        std::optional<OpeningShape> GetAnalOpening();
        void UpdateShafts();
        GeometryMath::Segment GetCrotchSegment() const;

      private:
        std::optional<TrackedOpening> trackedVagina;
        std::optional<TrackedOpening> trackedAnus;
    };

}
