#pragma once

#include "NiSnapshot.h"
#include "Node.h"
#include "Registry/Define/Sex.h"
#include "Util/RingBuffer.h"

namespace Thread::NiNode
{
	constexpr size_t WINDOW_SIZE = 30;

	struct NiActor
	{
		NiActor(RE::Actor* a_owner, Registry::Sex a_sex) :
			actor(a_owner), nodes(a_owner, a_sex != Registry::Sex::Female), sex(a_sex) {}
		~NiActor() = default;

		inline void CaptureSnapshot(float a_timeStamp) { snapshots.push(Snapshot{ nodes, a_timeStamp }); }

	  public:
		RE::ActorPtr actor;
		Node::NodeData nodes;
		REX::EnumSet<Registry::Sex> sex;
		Util::RingBuffer<Snapshot, WINDOW_SIZE> snapshots;
	};

}  // namespace Thread::NiNode
