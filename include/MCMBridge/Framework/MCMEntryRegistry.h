#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Framework/RendererCallbacks.h"

#include <mutex>
#include <span>
#include <unordered_map>

namespace MCMBridge
{
	class MCMEntryRegistry
	{
	public:
		static MCMEntryRegistry& GetSingleton();

		void Synchronize(std::span<const MCMMod> a_mods);
		void Render(std::size_t a_slot) const;

	private:
		MCMEntryRegistry();
		RendererCallbacks renderers;
		struct Entry
		{
			std::string modID;
			std::string pageID;
			std::string pageLabel;
			std::string sectionSegment;
			bool        active{};
			std::string rawName;
		};

		struct ModEntry
		{
			std::string sectionSegment;
			bool        active{};
			bool        grouped{};
			std::string range;
		};

		mutable std::mutex                           mutex;
		std::vector<Entry>                           entries;
		std::unordered_map<std::string, std::size_t> slotsByID;
		std::unordered_map<std::string, ModEntry>    mods;
		std::vector<std::string>                     structure;
		bool                                         structureValid{};
		bool                                         folderCreated{};
		std::vector<std::string>                     placement;
	};
}
