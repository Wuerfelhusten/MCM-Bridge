#include "MCMBridge/UI/OriginalPageNavigator.h"

#include "MCMBridge/Plugin/JournalRedirect.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "SKSEMenuFramework.h"

#include "RE/Skyrim.h"

namespace
{
	constexpr auto          panelRoot = "_root.ConfigPanelFader.configPanel";
	constexpr auto          retryDelay = std::chrono::milliseconds(100);
	constexpr std::uint32_t maximumAttempts = 60;

	struct EntryMatch
	{
		RE::GFxValue  entry;
		std::uint32_t listIndex{};
	};

	bool IsPanelReady(RE::GFxMovieView& a_movie)
	{
		RE::GFxValue state;
		return a_movie.GetVariable(std::addressof(state), "_root.ConfigPanelFader.configPanel._state") &&
		       state.IsNumber() && state.GetNumber() == 0.0;
	}

	std::optional<EntryMatch> FindNumberedEntry(
		RE::GFxMovieView& a_movie,
		const char*       a_entriesPath,
		const char*       a_memberName,
		std::int32_t      a_expected)
	{
		RE::GFxValue entries;
		if (!a_movie.GetVariable(std::addressof(entries), a_entriesPath) || !entries.IsArray()) {
			return std::nullopt;
		}
		for (std::uint32_t index = 0; index < entries.GetArraySize(); ++index) {
			RE::GFxValue entry;
			RE::GFxValue member;
			if (entries.GetElement(index, std::addressof(entry)) && entry.IsObject() &&
				entry.GetMember(a_memberName, std::addressof(member)) && member.IsNumber() &&
				static_cast<std::int32_t>(member.GetNumber()) == a_expected) {
				return EntryMatch{ std::move(entry), index };
			}
		}
		return std::nullopt;
	}

	bool SelectListEntry(
		RE::GFxMovieView& a_movie,
		const char*       a_listPath,
		const char*       a_selectionMethod,
		const EntryMatch& a_match)
	{
		RE::GFxValue enabled;
		if (a_match.entry.GetMember("enabled", &enabled) && enabled.IsBool() && !enabled.GetBool())
			return false;
		const RE::GFxValue selectedIndex(static_cast<double>(a_match.listIndex));
		const auto         selectedPath = std::format("{}.selectedIndex", a_listPath);
		const auto         updatePath = std::format("{}.UpdateList", a_listPath);
		if (!a_movie.SetVariable(selectedPath.c_str(), selectedIndex, RE::GFxMovie::SetVarType::kNormal)) {
			return false;
		}
		a_movie.Invoke(updatePath.c_str(), nullptr, nullptr, 0);
		const std::array arguments{ a_match.entry };
		return a_movie.Invoke(a_selectionMethod, nullptr, arguments.data(), static_cast<std::uint32_t>(arguments.size()));
	}

	std::optional<EntryMatch> FindRegistryEntry(RE::GFxMovieView& a_movie, std::string_view a_id)
	{
		RE::GFxValue entries;
		if (!a_movie.GetVariable(&entries, "_root.ConfigPanelFader.configPanel._modList.entryList") || !entries.IsArray())
			return {};
		std::optional<EntryMatch> result;
		for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i) {
			RE::GFxValue entry;
			RE::GFxValue key;
			if (!entries.GetElement(i, &entry) || !entry.IsObject() || !entry.GetMember("modName", &key) || !key.IsString())
				continue;
			if (std::string_view(key.GetString()) != a_id)
				continue;
			if (result)
				return {};
			result = EntryMatch{ entry, i };
		}
		return result;
	}
}

namespace MCMBridge
{
	OriginalPageNavigator& OriginalPageNavigator::GetSingleton()
	{
		static OriginalPageNavigator singleton;
		return singleton;
	}

	void OriginalPageNavigator::Open(OriginalPageTarget a_target)
	{
		const auto requestGeneration = ++generation;
		if (auto* window = SKSEMenuFramework::GetMainWindow()) {
			window->IsOpen.store(false);
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
			if (auto* messages = RE::UIMessageQueue::GetSingleton()) {
				messages->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			}
		}
		Schedule(requestGeneration, std::move(a_target), Phase::kOpenPanel, 0);
	}

	void OriginalPageNavigator::Cancel()
	{
		++generation;
	}

	void OriginalPageNavigator::Advance(
		std::uint64_t      a_generation,
		OriginalPageTarget a_target,
		Phase              a_phase,
		std::uint32_t      a_attempt)
	{
		if (a_generation != generation.load()) {
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		auto  movie = ui ? ui->GetMovieView(RE::JournalMenu::MENU_NAME) : nullptr;
		if (!ui || !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) || !movie) {
			Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
			return;
		}

		if (a_phase == Phase::kOpenPanel) {
			if (!movie->IsAvailable(panelRoot)) {
				Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
				return;
			}
			const RE::GFxValue systemTab(2.0);
			movie->SetVariable("_root.QuestJournalFader.Menu_mc.iCurrentTab", systemTab, RE::GFxMovie::SetVarType::kNormal);
			const std::array switchArguments{ systemTab, RE::GFxValue(false) };
			movie->Invoke("_root.QuestJournalFader.Menu_mc.SwitchPageToFront", nullptr,
				switchArguments.data(), static_cast<std::uint32_t>(switchArguments.size()));
			if (!JournalRedirect::OpenOriginalPanel(*movie)) {
				Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
				return;
			}
			Schedule(a_generation, std::move(a_target), Phase::kSelectMod, 0);
			return;
		}

		if (!IsPanelReady(*movie)) {
			Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
			return;
		}
		if (a_phase == Phase::kSelectMod) {
			auto match = a_target.registryID.empty() ? FindNumberedEntry(*movie,
														   "_root.ConfigPanelFader.configPanel._modList.entryList", "modIndex", a_target.configIndex) :
			                                           FindRegistryEntry(*movie, a_target.registryID);
			if (!match || !SelectListEntry(*movie, "_root.ConfigPanelFader.configPanel._modList",
							  "_root.ConfigPanelFader.configPanel.selectMod", *match)) {
				Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
				return;
			}
			if (a_target.pageIndex >= 0) {
				Schedule(a_generation, std::move(a_target), Phase::kSelectPage, 0);
			} else {
				SKSE::log::info("Opened original MCM root for config index {}", a_target.configIndex);
			}
			return;
		}

		auto         match = FindNumberedEntry(*movie,
			"_root.ConfigPanelFader.configPanel._subList.entryList", "pageIndex", a_target.pageIndex);
		RE::GFxValue pageName;
		if (match && (!match->entry.GetMember("pageName", &pageName) || !pageName.IsString() ||
						 std::string_view(pageName.GetString()) != a_target.pageName))
			match.reset();
		if (!match || !SelectListEntry(*movie, "_root.ConfigPanelFader.configPanel._subList",
						  "_root.ConfigPanelFader.configPanel.selectPage", *match)) {
			Schedule(a_generation, std::move(a_target), a_phase, a_attempt + 1);
			return;
		}
		SKSE::log::info("Opened original MCM page {} for config index {}", a_target.pageName, a_target.configIndex);
	}

	void OriginalPageNavigator::Schedule(
		std::uint64_t      a_generation,
		OriginalPageTarget a_target,
		Phase              a_phase,
		std::uint32_t      a_attempt)
	{
		if (a_generation != generation.load()) {
			return;
		}
		if (a_attempt >= maximumAttempts) {
			SKSE::log::error("Could not open the original MCM page within the navigation timeout");
			WriteNotifications::Show("Could not open the original MCM page. It may be unavailable or disabled in the original menu.");
			return;
		}
		TaskScheduler::GetSingleton().After(retryDelay,
			[a_generation, target = std::move(a_target), a_phase, a_attempt]() mutable {
				GetSingleton().Advance(a_generation, std::move(target), a_phase, a_attempt);
			});
	}
}
