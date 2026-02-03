#include "NiSnapshot.h"

#include "NiMath.h"

namespace Thread::NiNode
{
	Snapshot::Snapshot(const Node::NodeData& nodes, float a_timeStamp) :
		timeStamp(a_timeStamp)
	{
		if (const auto niHead = nodes.head) {
			anchors[Anchor::vHeadY] = niHead->world.rotate.GetVectorY();
			anchors[Anchor::vHeadZ] = niHead->world.rotate.GetVectorZ();
			anchors[Anchor::pHead] = nodes.head->world.translate;
			if (auto opt = ObjectBound::MakeBoundingBox(niHead.get())) {
				headBounds = *opt;
				const auto down = headBounds.boundMin.z * 0.17f;
				const auto forward = headBounds.boundMax.y * 0.88f;
				anchors[Anchor::pThroat] = (anchors[Anchor::vHeadZ] * down) + anchors[Anchor::pHead];
				anchors[Anchor::pMouth] = (anchors[Anchor::vHeadY] * forward) + anchors[Anchor::pThroat];
			} else {
				headBounds = ObjectBound{};
				logger::warn("Failed to get head bounding box");
			}
		}
	}

}  // namespace Thread::NiNode
