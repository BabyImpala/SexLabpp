#pragma once

#include "NiMotion.h"

namespace Thread::Interaction::NiML
{
    class FeatureSet
    {
      public:
        enum class Feature : uint32_t
        {
            //---------------------------------------------------------------------
            // Geometry
            //---------------------------------------------------------------------

            Distance = 1 << 0,                  // "How close are the two reference points?"
            AlongTargetAxisDistance = 1 << 1,   // "How far along the target's axis is the effector?"
            AcrossTargetAxisDistance = 1 << 2,  // "How far away from the target's axis is the effector?"

            //---------------------------------------------------------------------
            // Speed
            //---------------------------------------------------------------------

            EffectorSpeed = 1 << 3,  // "How fast is the effector moving?"
            TargetSpeed = 1 << 4,    // "How fast is the target moving?"

            AverageSpeed = 1 << 5,     // "How much are they moving overall?"
            MaximumSpeed = 1 << 6,     // "Is either participant moving quickly?"
            DifferenceSpeed = 1 << 7,  // "Are they moving at similar speeds?"

            //---------------------------------------------------------------------
            // Oscillation
            //---------------------------------------------------------------------

            EffectorOscillation = 1 << 8,  // "How rhythmic is the effector?"
            TargetOscillation = 1 << 9,    // "How rhythmic is the target?"

            AverageOscillation = 1 << 10,     // "How rhythmic is the interaction overall?"
            MaximumOscillation = 1 << 11,     // "Is either participant oscillating strongly?"
            DifferenceOscillation = 1 << 12,  // "Are both oscillating similarly?"

            //---------------------------------------------------------------------
            // Positional stability
            //---------------------------------------------------------------------

            EffectorPositionalVariance = 1 << 13,  // "How much does the effector wander?"
            TargetPositionalVariance = 1 << 14,    // "How much does the target wander?"

            AveragePositionalVariance = 1 << 15,     // "How spatially stable is the interaction?"
            MaximumPositionalVariance = 1 << 16,     // "Is either participant spatially unstable?"
            DifferencePositionalVariance = 1 << 17,  // "Do both participants exhibit similar stability?"

            //---------------------------------------------------------------------
            // Directional stability
            //---------------------------------------------------------------------

            EffectorDirectionalVariance = 1 << 18,  // "How consistently does the effector move?"
            TargetDirectionalVariance = 1 << 19,    // "How consistently does the target move?"

            AverageDirectionalVariance = 1 << 20,     // "How directionally stable is the interaction?"
            MaximumDirectionalVariance = 1 << 21,     // "Is either participant changing direction frequently?"
            DifferenceDirectionalVariance = 1 << 22,  // "Do both participants change direction similarly?"

            //---------------------------------------------------------------------
            // Relative motion
            //---------------------------------------------------------------------

            RelativeMotionAlongAxis = 1 << 23,      // "Is the relative motion primarily along the target axis?"
            RelativeMotionAcrossAxis = 1 << 24,     // "Is the relative motion primarily across the target axis?"
            TrajectoryAlignment = 1 << 25,          // "Are both trajectories parallel?"
            TrajectoryAlignmentAbsolute = 1 << 26,  // "Are both trajectories parallel? (Ignoring direction)"

            //---------------------------------------------------------------------
            // Orientation
            //---------------------------------------------------------------------

            EffectorFacingTarget = 1 << 27,  // "Is the effector facing the target?"
            TargetFacingEffector = 1 << 28,  // "Is the target facing the effector?"
            AxisAlignment = 1 << 29,         // "Are the primary axes aligned?"
            PlaneAlignment = 1 << 30,        // "Are the secondary axes aligned?"
        };
        static constexpr size_t NUM_FEATURES = magic_enum::enum_count<Feature>();

        struct ReferenceData
        {
            NiMotion* motion;
            NiMotion::Anchor anchor;
            NiMotion::Anchor axis;
            std::optional<NiMotion::Anchor> secondaryAxis;
        };

      public:
        FeatureSet(ReferenceData& frameA, ReferenceData& frameB, REX::EnumSet<Feature> features);
        ~FeatureSet() = default;

        REX::EnumSet<Feature> GetFeatures() const { return _features; }
        bool HasFeature(Feature feature) const { return _features.all(feature); }

        /// @brief Get a feature vector for the given reference frames
        /// @return Vector of feature values, in the order defined by Feature enum
        std::vector<float> GetFeatureVector() const;

        /// @brief Get the value of a specific feature
        /// @param feature Feature to retrieve
        /// @return Value of the specified feature
        /// @throws std::invalid_argument if the feature is not present in the feature set
        float GetFeatureValue(Feature feature) const;

        /// @brief Get the value of a specific feature, computing it if necessary
        /// @param feature Feature to retrieve
        /// @return Value of the specified feature
        float GetFeatureValue(Feature feature);

      private:
        struct WorkingData
        {
            WorkingData(FeatureSet::ReferenceData& frameA, FeatureSet::ReferenceData& frameB);
            ~WorkingData() = default;

            const MotionDescriptor& motionA;
            const MotionDescriptor& motionB;
            RE::NiPoint3 pA;
            RE::NiPoint3 pB;
            RE::NiPoint3 targetAxis;
            RE::NiPoint3 effectorAxis;
            RE::NiPoint3 offset;
            float distance;
            float alongTargetAxis;
            RE::NiPoint3 dirToTarget;
            RE::NiPoint3 trajA;
            RE::NiPoint3 trajB;
            RE::NiPoint3 relativeMotion;
            float relativeAlongAxis;
            RE::NiPoint3 secondaryAxisA;
            RE::NiPoint3 secondaryAxisB;

            inline float ComputeSimilarity(float a, float b) const;
            float ComputeFeature(FeatureSet::Feature feature) const;
        };

        WorkingData _data;
        REX::EnumSet<Feature> _features;
        std::array<std::optional<float>, NUM_FEATURES> _featureValues{};
    };

    using Feature = FeatureSet::Feature;

}  // namespace Thread::Interaction::NiML