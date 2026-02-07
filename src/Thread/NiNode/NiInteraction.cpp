#include "NiInteraction.h"

#include "NiMath.h"

namespace Thread::NiNode
{
	NiInteraction EvaluateKissing(const NiMotion& a_motionA, const NiMotion& a_motionB)
	{
		NiInteraction result{ NiInteraction::Type::Kissing };
		assert(a_motionA.HasSufficientData() && a_motionB.HasSufficientData());

		const auto mouthA = a_motionA.DescribeMotion(NiMotion::pMouth);
		const auto mouthB = a_motionB.DescribeMotion(NiMotion::pMouth);
		const float mouthDistance = mouthA.Mean().GetDistance(mouthB.Mean());
		if (mouthDistance > Settings::fDistanceMouth * 2.0f) {
			return result;
		}

		const float duration = std::min(mouthA.duration, mouthB.duration);
		const auto vHeadYA = a_motionA.GetLatestMoment(NiMotion::vHeadY);
		const auto vHeadYB = a_motionB.GetLatestMoment(NiMotion::vHeadY);
		const float cosFacingAngle = NiMath::GetCosAngle(vHeadYA, vHeadYB);  // Facing each other <=> cos ~ -1

		const float facingScore = std::clamp(-cosFacingAngle, 0.0f, 1.0f);
		const float distanceScore = 1.0f - std::clamp(mouthDistance / Settings::fDistanceMouth, 0.0f, 1.0f);
		const float avgVelocity = 0.5f * (mouthA.avgSpeed + mouthB.avgSpeed);
		const float velocityScore = 1.0f - std::clamp(avgVelocity / Settings::fMaxKissSpeed, 0.0f, 1.0f);
		const float oscillationScore = 1.0f - std::clamp(0.5f * (mouthA.oscillation + mouthB.oscillation), 0.0f, 1.0f);
		const float impulseScore = 1.0f - std::clamp(0.5f * (mouthA.impulse + mouthB.impulse), 0.0f, 1.0f);
		const float timeScore = std::clamp(duration / Settings::fMinKissDuration, 0.0f, 1.0f);

		KissingDescriptor descriptor{};
		descriptor.AddValue(Feature::Distance, distanceScore);
		descriptor.AddValue(Feature::Facing, facingScore);
		descriptor.AddValue(Feature::Time, timeScore);
		descriptor.AddValue(Feature::Velocity, velocityScore);
		descriptor.AddValue(Feature::Oscillation, oscillationScore);
		descriptor.AddValue(Feature::Impulse, impulseScore);

		result.confidence = descriptor.Predict();
		result.csvRow = descriptor.ToString();
		result.duration = duration;
		result.velocity = avgVelocity;
		result.active = result.confidence > Settings::fEnterThreshold;

		return result;
	}

}  // namespace Thread::NiNode
