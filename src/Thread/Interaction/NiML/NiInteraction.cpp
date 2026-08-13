#include "NiInteraction.h"

#include "NiMath.h"
#include "FeatureSet.h"

namespace Thread::Interaction::NiML
{
    bool NiInteractionCluster::IncludesType(NiType::Type type) const
    {
        return std::ranges::any_of(interactions, [type](const NiInteraction& interaction) {
            return interaction.GetType() == type;
        });
    }

    NiType::Cluster NiInteractionCluster::GetClusterType() const
    {
        if (interactions.empty()) {
            return NiType::Cluster::None;
        }
        return NiType::GetClusterForType(interactions.front().GetType());
    }

    NiInteraction* NiInteractionCluster::ApplySoftmax()
    {
        if (interactions.empty()) {
            return nullptr;
        } else if (interactions.size() == 1) {
            return &interactions.front();
        }

        std::vector<float> logits;
        logits.reserve(interactions.size());
        for (auto& interaction : interactions) {
            if (!interaction.descriptor)
                logits.push_back(-std::numeric_limits<float>::infinity());
            else
                logits.push_back(interaction.descriptor->Predict());
        }
        float maxLogit = *std::max_element(logits.begin(), logits.end());

        std::vector<float> expVals;
        expVals.reserve(logits.size());

        float sum = 0.0f;
        for (float z : logits) {
            float e = std::exp(z - maxLogit);
            expVals.push_back(e);
            sum += e;
        }
        for (float& e : expVals)
            e /= sum;

        int bestIndex = 0;
        float bestProb = expVals[0];
        for (int i = 1; i < expVals.size(); ++i) {
            if (expVals[i] > bestProb) {
                bestProb = expVals[i];
                bestIndex = i;
            }
        }

        if (bestProb < Settings::fEnterThresholdSoftmax) {
            return nullptr;
        }

        return &interactions[bestIndex];
    }

    std::string NiInteractionCluster::GetCsvFeatureHeader() const
    {
        if (interactions.empty()) {
            return "";
        }
        assert(interactions.front().descriptor);
        return interactions.front().descriptor->GetCsvFeatureHeader();
    }

    std::string NiInteractionCluster::GetCsvFeatureRow() const
    {
        if (interactions.empty()) {
            return "";
        }
        assert(interactions.front().descriptor);
        return interactions.front().descriptor->GetCsvFeatureRow();
    }

#define ADD_TYPE(t, f, v) result.interactions.emplace_back(std::make_unique<NiDescriptor<NiType::Type::t>>(f), v);

    NiInteractionCluster EvaluateCrotchInteractions(NiMotion& a_motionA, NiMotion& a_motionB)
    {
        // a_motionA: receiving actor (vagina/anal/grinding)
        // a_motionB: penetrating actor (with schlong)
        NiInteractionCluster result{};
        assert(a_motionA.HasSufficientData() && a_motionB.HasSufficientData());
        if (!a_motionB.HasMomentData(NiMotion::pSchlongTip) || !a_motionA.HasMomentData(NiMotion::pSchlongBase)) {
            return result;
        } else if (!a_motionA.HasMomentData(NiMotion::pVaginalStart) || !a_motionA.HasMomentData(NiMotion::pAnalStart)) {
            return result;
        }

        const auto pSchlongTip = a_motionB.GetLatestMoment(NiMotion::pSchlongTip);
        const auto pVaginalStart = a_motionA.GetLatestMoment(NiMotion::pVaginalStart);
        const auto pAnalStart = a_motionA.GetLatestMoment(NiMotion::pAnalStart);
        const auto distance = std::min(pSchlongTip.GetDistance(pVaginalStart), pSchlongTip.GetDistance(pAnalStart));
        if (distance > Settings::fDistanceCrotch * 2.0f) {
            return result;
        }

        REX::EnumSet<Feature> features{
            Feature::Distance,
            Feature::AlongTargetAxisDistance,
            Feature::AcrossTargetAxisDistance,
            Feature::MaximumOscillation,
            Feature::RelativeMotionAlongAxis,
            Feature::RelativeMotionAcrossAxis,
            Feature::EffectorFacingTarget,
            Feature::TargetFacingEffector,
            Feature::AxisAlignment,
        };

        FeatureSet::ReferenceData effectorVag{
            .motion = &a_motionA,
            .anchor = NiMotion::pVaginalStart,
            .axis = NiMotion::vVaginal,
        };
        FeatureSet::ReferenceData effectorAn{
            .motion = &a_motionA,
            .anchor = NiMotion::pAnalStart,
            .axis = NiMotion::vAnal,
        };

        FeatureSet::ReferenceData target{
            .motion = &a_motionB,
            .anchor = NiMotion::pSchlongBase,
            .axis = NiMotion::vSchlong,
        };

        const auto setVag = FeatureSet{ effectorVag, target, features };
        const auto setAnal = FeatureSet{ effectorAn, target, features };

        auto featureVector = setVag.GetFeatureVector();
        const auto featureVectorAnal = setAnal.GetFeatureVector();
        featureVector.insert(featureVector.end(), featureVectorAnal.begin(), featureVectorAnal.end());
        const auto avgVelocity = (setVag.GetFeatureValue(Feature::AverageSpeed) + setAnal.GetFeatureValue(Feature::AverageSpeed)) / 2.0f;

        ADD_TYPE(Vaginal, featureVector, avgVelocity)
        ADD_TYPE(Anal, featureVector, avgVelocity)
        ADD_TYPE(Grinding, featureVector, avgVelocity)
        ADD_TYPE(Crotch_NONE, featureVector, avgVelocity)

        return result;
    }

    NiInteractionCluster EvaluateHeadInteractions(NiMotion& a_motionA, NiMotion& a_motionB)
    {
        // a_motionA: actor using head (mouth/throat)
        // a_motionB: target actor (e.g., with schlong)
        NiInteractionCluster result{};
        assert(a_motionA.HasSufficientData() && a_motionB.HasSufficientData());

        const auto headBound = a_motionA.GetLatestHeadBound();
        if (!a_motionB.HasMomentData(NiMotion::pSchlongBase) || !headBound.IsValid()) {
            return result;
        }

        const auto headEntryMotion = a_motionA.GetMotionDescriptor(NiMotion::pMouth);
        const auto schlongBaseMotion = a_motionB.GetMotionDescriptor(NiMotion::pSchlongBase);
        if (!headEntryMotion.DescribesMotion() && !schlongBaseMotion.DescribesMotion()) {
            return result;
        }

        const auto pHead = a_motionB.GetLatestMoment(NiMotion::pHead);
        const auto pSchlongEnd = a_motionB.GetLatestMoment(NiMotion::pSchlongTip);
        const float distanceSkull = pSchlongEnd.GetDistance(pHead);
        if (distanceSkull > Settings::fCloseToHead * 2.0f) {
            return result;
        }
        
        REX::EnumSet<Feature> features{
            Feature::Distance,
            Feature::AlongTargetAxisDistance,
            Feature::AcrossTargetAxisDistance,
            Feature::MaximumOscillation,
            Feature::RelativeMotionAlongAxis,
            Feature::RelativeMotionAcrossAxis,
            Feature::EffectorFacingTarget,
            Feature::TargetFacingEffector,
            Feature::AxisAlignment,
        };

        FeatureSet::ReferenceData effector{
            .motion = &a_motionA,
            .anchor = NiMotion::pMouth,
            .axis = NiMotion::vHeadY,
        };

        FeatureSet::ReferenceData target{
            .motion = &a_motionB,
            .anchor = NiMotion::pSchlongBase,
            .axis = NiMotion::vSchlong,
        };

        const FeatureSet featureSet{ effector, target, features };
        const auto featureVector = featureSet.GetFeatureVector();
        const auto velocity = featureSet.GetFeatureValue(Feature::AverageSpeed);

        ADD_TYPE(Oral, featureVector, velocity)
        ADD_TYPE(Deepthroat, featureVector, velocity)
        ADD_TYPE(Skullfuck, featureVector, velocity)
        ADD_TYPE(LickingShaft, featureVector, velocity)
        ADD_TYPE(Head_NONE, featureVector, velocity)

        return result;
    }

    NiInteractionCluster EvaluateKissingCluster(NiMotion& a_motionA, NiMotion& a_motionB)
    {
        NiInteractionCluster result{};
        assert(a_motionA.HasSufficientData() && a_motionB.HasSufficientData());

        const auto mouthA = a_motionA.GetMotionDescriptor(NiMotion::pMouth);
        const auto mouthB = a_motionB.GetMotionDescriptor(NiMotion::pMouth);
        const float mouthDistance = mouthA.Mean().GetDistance(mouthB.Mean());
        if (mouthDistance > Settings::fDistanceMouth * 2.0f) {
            return result;
        }

        FeatureSet::ReferenceData effector{
            .motion = &a_motionA,
            .anchor = NiMotion::pMouth,
            .axis = NiMotion::vHeadY,
        };

        FeatureSet::ReferenceData target{
            .motion = &a_motionB,
            .anchor = NiMotion::pMouth,
            .axis = NiMotion::vHeadY,
        };

        FeatureSet featureSet{
            effector,
            target,
            {
                Feature::Distance,
                Feature::AverageSpeed,
                Feature::DifferenceSpeed,
                Feature::AverageOscillation,
                Feature::TrajectoryAlignment,
                Feature::EffectorFacingTarget,
                Feature::TargetFacingEffector,
                Feature::AxisAlignment,
            }
        };

        const auto features = featureSet.GetFeatureVector();
        const auto avgVelocity = featureSet.GetFeatureValue(Feature::AverageSpeed);

        ADD_TYPE(Kissing, features, avgVelocity)

        return result;
    }

#undef ADD_TYPE
}  // namespace Thread::Interaction::NiML
