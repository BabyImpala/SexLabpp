#pragma once

#include "ActorGeometry.h"
#include "Registry/Define/Sex.h"
#include "Registry/Util/RayCast/ObjectBound.h"

namespace Thread::NiNode::Surface
{
    /// Rotate a node so its reference segment points toward the target, subject to the maximum adjustment.
    bool RotateNode(RE::NiPointer<RE::NiNode> a_node, const GeometryMath::Segment& a_segment, const RE::NiPoint3& a_target, float a_maxAngle);

    struct Interaction
    {
        enum class Action
        {
            None = 0,
            Vaginal,
            Anal,
            Oral,
            Grinding,
            Deepthroat,
            Skullfuck,
            LickingShaft,
            FootJob,
            HandJob,
            Kissing,
            Facial,
            AnimObjFace,
            ToeSucking,

            Total,
        };

        Interaction(RE::ActorPtr a_partner, Action a_action, float a_distance) :
          partner(a_partner), action(a_action), distance(a_distance) {}

        RE::ActorPtr partner{ 0 };
        Action action{ Action::None };
        float distance{ 0.0f };
        float velocity{ 0.0f };

        bool operator==(const Interaction& a_rhs) const { return a_rhs.partner == partner && a_rhs.action == action; }
        bool operator<(const Interaction& a_rhs) const
        {
            const auto cmp = partner->GetFormID() <=> a_rhs.partner->GetFormID();
            return cmp == 0 ? action < a_rhs.action : cmp < 0;
        }
    };

    struct ActorState
    {
        struct Frame
        {
            explicit Frame(ActorState& a_state);

            bool DetectShaftHead(const Frame& a_partner, const Geometry::Shaft& a_shaft);
            bool DetectShaftCrotch(const Frame& a_partner, const Geometry::Shaft& a_shaft);
            bool DetectShaftHand(const Frame& a_partner, const Geometry::Shaft& a_shaft);
            bool DetectShaftFoot(const Frame& a_partner, const Geometry::Shaft& a_shaft);
            bool DetectVaginalOral(const Frame& a_partner);
            bool DetectVaginalContact(const Frame& a_partner);
            bool DetectVaginalLimb(const Frame& a_partner);
            bool DetectKissing(const Frame& a_partner);
            bool DetectToeSucking(const Frame& a_partner);
            bool DetectAnimObjectFace(const Frame& a_partner);

            std::optional<RE::NiPoint3> GetMouthCenter() const;
            std::optional<RE::NiPoint3> GetThroatPoint() const;

            ActorState& state;
            ObjectBound headBounds;
            std::optional<OpeningShape> mouthOpening;
            std::optional<OpeningShape> vaginalOpening;
            std::optional<OpeningShape> analOpening;
            std::vector<Interaction> interactions{};

            bool operator==(const Frame& a_rhs) const { return state == a_rhs.state; }
        };

        ActorState(RE::Actor* a_owner, Registry::Sex a_sex) :
          actor(a_owner), geometry(a_owner), sex(a_sex) {}

        RE::ActorPtr actor;
        Geometry::ActorGeometry geometry;
        stl::enumeration<Registry::Sex> sex;
        std::set<Interaction> interactions{};

        bool operator==(const ActorState& a_rhs) const { return actor == a_rhs.actor; }
    };

}  // namespace Thread::NiNode::Surface
