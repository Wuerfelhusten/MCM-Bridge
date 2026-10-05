#pragma once

#include "MCMBridge/Core/HostControlCapture.h"

namespace MCMBridge
{
	// Owned by one game-thread host session. UI deltas do not establish persisted
	// setting values; post-close confirmation still requires an authoritative read.
	class HostPageState
	{
	public:
		void                      Reconcile(std::string a_page, std::int32_t a_index, ClassicPageBuffers a_buffers);
		bool                      Apply(const HostControlChanges& a_changes);
		void                      Invalidate() { current = false; }
		const ClassicPageBuffers* Get(std::string_view a_page, std::int32_t a_index) const;
		std::uint64_t             StructureRevision() const { return structureRevision; }
		std::uint64_t             ValueRevision() const { return valueRevision; }

	private:
		bool               current{};
		std::string        page;
		std::int32_t       index{};
		ClassicPageBuffers baseline;
		ClassicPageBuffers observed;
		std::uint64_t      structureRevision{};
		std::uint64_t      valueRevision{};
	};
}
