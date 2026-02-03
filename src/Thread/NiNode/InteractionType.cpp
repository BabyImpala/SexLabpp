#include "InteractionType.h"

namespace Thread::NiNode
{
	InteractionType EvaluateKissing(const NiActor& a_self, const NiActor& a_other)
	{
		InteractionType result{};

		if (a_self.snapshots.size() < 3 || a_other.snapshots.size() < 3)
			return result;

		float accumulatedTime = 0.0f;
		float weightedDistance = 0.0f;
		float weightedFacing = 0.0f;
		int validFrames = 0;

		for (size_t i = 1; i < a_self.snapshots.size(); i++) {
			const auto& a0 = a_self.snapshots[i - 1];
			const auto& a1 = a_self.snapshots[i];
			const auto& b0 = a_other.snapshots[i - 1];
			const auto& b1 = a_other.snapshots[i];
			const float dt = a1.GetTimeStamp() - a0.GetTimeStamp();
			if (dt <= 0.0f)
				continue;

			const float distance = a1.Get(Snapshot::Anchor::pMouth).GetDistance(b1.Get(Snapshot::Anchor::pMouth));
			if (distance > Settings::fDistanceMouth)
				continue;

			const RE::NiPoint3& vA = a1.Get(Snapshot::Anchor::vHeadY);
			const RE::NiPoint3& vB = b1.Get(Snapshot::Anchor::vHeadY);

			float dot = vA.Dot(vB);
			float len = vA.Length() * vB.Length();
			if (len <= 1e-4f)
				continue;

			float cosAngle = dot / len;
			// kissing <=> facing each other <=> cos ~ -1
			float facingScore = std::clamp((-cosAngle), 0.0f, 1.0f);

			accumulatedTime += dt;
			weightedDistance += distance;
			weightedFacing += facingScore;
			validFrames++;
		}

		if (validFrames == 0)
			return result;

		const float avgDistance = weightedDistance / validFrames;
		const float avgFacing = weightedFacing / validFrames;

		const float distanceScore = 1.0f - std::clamp(avgDistance / Settings::fDistanceMouth, 0.0f, 1.0f);
		const float timeScore = std::clamp(accumulatedTime / Settings::fMinKissDuration, 0.0f, 1.0f);

		// COMEBACK: abstract weights behind a learned linear model
		result.confidence =
			0.45f * distanceScore +
			0.35f * avgFacing +
			0.20f * timeScore;

		result.duration = accumulatedTime;

		return result;
	}

}  // namespace Thread::NiNode
