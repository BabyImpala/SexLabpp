#pragma once

#include "NiActor.h"

namespace Thread::NiNode
{
	struct InteractionType
	{
		enum class Type
		{
			None = 0,
			Kissing,
			Choking,
		} type{ Type::None };
		float confidence{ 0.0f };		  // 0..1
		float duration{ 0.0f };			  // ms? COMEBACK: check units
		bool active{ false };

		constexpr static inline size_t NUM_TYPES = magic_enum::enum_count<Type>();
	};

	InteractionType EvaluateKissing(const NiActor& a_self, const NiActor& a_other);

}  // namespace Thread::NiNode
