#pragma once

#include "Node.h"
#include "Registry/Util/RayCast/ObjectBound.h"

namespace Thread::NiNode
{
	struct Snapshot
	{
		enum Anchor
		{
			vHeadY,
			vHeadZ,
			pMouth,
			pHead,
			pThroat,
		};
		constexpr static inline size_t NUM_ANCHORS = magic_enum::enum_count<Anchor>();

	  public:
		Snapshot(const Node::NodeData& a_nodes, float a_timeStamp);
		~Snapshot() = default;

		inline const RE::NiPoint3& Get(const Anchor a) const { return anchors[a]; }
		inline float GetTimeStamp() const { return timeStamp; }

		inline float GetDistance(const Anchor a, const RE::NiPoint3& b) const { return anchors[a].GetDistance(b); }
		inline float GetDistance(const Anchor a, const Anchor b) const { return GetDistance(a, anchors[b]); }

		inline RE::NiPoint3 GetVector(const Anchor a, const RE::NiPoint3& b) const { return b - anchors[a]; }
		inline RE::NiPoint3 GetVector(const Anchor a, const Anchor b) const { return GetVector(a, anchors[b]); }

	  private:
		std::array<RE::NiPoint3, NUM_ANCHORS> anchors;
		ObjectBound headBounds;
		float timeStamp;
	};

}  // namespace Thread::NiNode
