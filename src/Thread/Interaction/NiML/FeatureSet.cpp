#include "FeatureSet.h"

#include "NiMath.h"

namespace Thread::Interaction::NiML
{
    FeatureSet::WorkingData::WorkingData(FeatureSet::ReferenceData& frameA, FeatureSet::ReferenceData& frameB) :
      motionA(frameA.motion->GetMotionDescriptor(frameA.anchor)),
      motionB(frameB.motion->GetMotionDescriptor(frameB.anchor)),
      pA(motionA.Mean()),
      pB(motionB.Mean()),
      targetAxis(frameB.motion->GetLatestMoment(frameB.axis)),
      effectorAxis(frameA.motion->GetLatestMoment(frameA.axis)),
      offset(pA - pB),
      distance(offset.Length()),
      alongTargetAxis(offset.Dot(targetAxis)),
      dirToTarget(distance > FLT_EPSILON ? -offset / distance : RE::NiPoint3::Zero()),
      trajA(motionA.trajectory.Vector()),
      trajB(motionB.trajectory.Vector()),
      relativeMotion(trajA - trajB),
      relativeAlongAxis(relativeMotion.Dot(targetAxis)),
      secondaryAxisA(frameA.secondaryAxis ? frameA.motion->GetLatestMoment(*frameA.secondaryAxis) : RE::NiPoint3::Zero()),
      secondaryAxisB(frameB.secondaryAxis ? frameB.motion->GetLatestMoment(*frameB.secondaryAxis) : RE::NiPoint3::Zero())
    {
        targetAxis.Unitize();
        effectorAxis.Unitize();
    }

    float FeatureSet::WorkingData::ComputeSimilarity(float a, float b) const
    {
        constexpr float epsilon = std::numeric_limits<float>::epsilon();
        return std::abs(a - b) / (a + b + epsilon);
    }

    float FeatureSet::WorkingData::ComputeFeature(FeatureSet::Feature feature) const
    {
        switch (feature) {
        case Feature::Distance:
            return distance;
        case Feature::AlongTargetAxisDistance:
            return alongTargetAxis;
        case Feature::AcrossTargetAxisDistance:
            {
                const RE::NiPoint3 perpendicular = offset - targetAxis * alongTargetAxis;
                return perpendicular.Length();
            }
        case Feature::EffectorSpeed:
            return motionA.avgSpeed;
        case Feature::TargetSpeed:
            return motionB.avgSpeed;
        case Feature::AverageSpeed:
            return (motionA.avgSpeed + motionB.avgSpeed) / 2.0f;
        case Feature::MaximumSpeed:
            return std::max(motionA.avgSpeed, motionB.avgSpeed);
        case Feature::DifferenceSpeed:
            return ComputeSimilarity(motionA.avgSpeed, motionB.avgSpeed);
        case Feature::EffectorOscillation:
            return motionA.oscillation;
        case Feature::TargetOscillation:
            return motionB.oscillation;
        case Feature::AverageOscillation:
            return (motionA.oscillation + motionB.oscillation) / 2.0f;
        case Feature::MaximumOscillation:
            return std::max(motionA.oscillation, motionB.oscillation);
        case Feature::DifferenceOscillation:
            return ComputeSimilarity(motionA.oscillation, motionB.oscillation);
        case Feature::EffectorPositionalVariance:
            return motionA.positionalVariance;
        case Feature::TargetPositionalVariance:
            return motionB.positionalVariance;
        case Feature::AveragePositionalVariance:
            return (motionA.positionalVariance + motionB.positionalVariance) / 2.0f;
        case Feature::MaximumPositionalVariance:
            return std::max(motionA.positionalVariance, motionB.positionalVariance);
        case Feature::DifferencePositionalVariance:
            return ComputeSimilarity(motionA.positionalVariance, motionB.positionalVariance);
        case Feature::EffectorDirectionalVariance:
            return motionA.directionalVariance;
        case Feature::TargetDirectionalVariance:
            return motionB.directionalVariance;
        case Feature::AverageDirectionalVariance:
            return (motionA.directionalVariance + motionB.directionalVariance) / 2.0f;
        case Feature::MaximumDirectionalVariance:
            return std::max(motionA.directionalVariance, motionB.directionalVariance);
        case Feature::DifferenceDirectionalVariance:
            return ComputeSimilarity(motionA.directionalVariance, motionB.directionalVariance);
        case Feature::RelativeMotionAlongAxis:
            return relativeAlongAxis;
        case Feature::RelativeMotionAcrossAxis:
            {
                RE::NiPoint3 perpendicular = relativeMotion - targetAxis * relativeAlongAxis;
                return perpendicular.Length();
            }
        case Feature::TrajectoryAlignment:
            return NiMath::GetAngleCos(trajA, trajB);
        case Feature::TrajectoryAlignmentAbsolute:
            return std::fabs(NiMath::GetAngleCos(trajA, trajB));
        case Feature::EffectorFacingTarget:
            return effectorAxis.Dot(dirToTarget);
        case Feature::TargetFacingEffector:
            return targetAxis.Dot(-dirToTarget);
        case Feature::AxisAlignment:
            return effectorAxis.Dot(-targetAxis);
        case Feature::PlaneAlignment:
            if (secondaryAxisA.SqrLength() > std::numeric_limits<float>::epsilon() && secondaryAxisB.SqrLength() > std::numeric_limits<float>::epsilon()) {
                return NiMath::GetAngleCos(secondaryAxisA, secondaryAxisB);
            } else {
                logger::warn("PlaneAlignment feature requires both reference frames to have a secondary axis defined.");
                return 0.0f;
            }
        default:
            logger::warn("Unhandled feature: {}", magic_enum::enum_name(feature));
            return 0.0f;
        }
    }

    FeatureSet::FeatureSet(ReferenceData& frameA, ReferenceData& frameB, REX::EnumSet<Feature> features) :
      _data(frameA, frameB), _features(features)
    {
        for (size_t i = 0; i < NUM_FEATURES; i++) {
            const auto feature = static_cast<Feature>(1 << i);
            if (!HasFeature(feature)) {
                continue;
            }
            const float value = _data.ComputeFeature(feature);
            _featureValues[i] = value;
        }
    }

    std::vector<float> FeatureSet::GetFeatureVector() const
    {
        std::vector<float> features;
        features.reserve(NUM_FEATURES);

        for (size_t i = 0; i < NUM_FEATURES; i++) {
            const auto feature = static_cast<Feature>(1 << i);
            if (!HasFeature(feature)) {
                continue;
            }
            const float value = GetFeatureValue(feature);
            features.push_back(value);
        }
        return features;
    }

    float FeatureSet::GetFeatureValue(Feature feature) const
    {
        if (!HasFeature(feature)) {
            throw std::invalid_argument("Feature not present in feature set");
        }

        const size_t index = static_cast<size_t>(std::log2(static_cast<uint32_t>(feature)));
        if (_featureValues[index].has_value()) {
            return _featureValues[index].value();
        } else {
            logger::error("Feature value for {} is not computed.", static_cast<uint32_t>(feature));
            assert(false && "Feature value not computed");
            return 0.0f;
        }
    }

    float FeatureSet::GetFeatureValue(Feature feature)
    {
        const size_t index = static_cast<size_t>(std::log2(static_cast<uint32_t>(feature)));
        if (_featureValues[index].has_value()) {
            return _featureValues[index].value();
        } else {
            _featureValues[index] = _data.ComputeFeature(feature);
            return _featureValues[index].value();
        }
    }

}  // namespace Thread::Interaction::NiML
