#include "MCMBridge/Plugin/NativeJournalEntry.h"

#include "MCMBridge/Core/NativeJournalEntry.h"

namespace
{
	constexpr auto marker = "_mcmBridgeEntry";
	constexpr auto originalPress = "_mcmBridgeOriginalCategoryPress";
	constexpr auto openMethod = "_mcmBridgeNativeOpen";

	class CategoryHandler final : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr)
				return;
			RE::GFxValue entry;
			RE::GFxValue owned;
			if (a_params.argCount && a_params.args[0].IsObject() &&
				a_params.args[0].GetMember("entry", &entry) && entry.IsObject() &&
				entry.GetMember(marker, &owned) && owned.IsBool() && owned.GetBool()) {
				RE::GFxValue state;
				RE::GFxValue disabled;
				if (!a_params.thisPtr->GetMember("iCurrentState", &state) || !state.IsNumber() || state.GetNumber() != 0 ||
					(entry.GetMember("disabled", &disabled) && disabled.IsBool() && disabled.GetBool()))
					return;
				a_params.movie->Invoke("_root.QuestJournalFader.Menu_mc._mcmBridgeNativeOpen", nullptr, nullptr, 0);
				return;
			}
			// Existing entries retain their original event index and callback exactly once.
			a_params.thisPtr->Invoke(originalPress, a_params.retVal, a_params.args, a_params.argCount);
		}
	};
}

namespace MCMBridge
{
	bool AttachNativeJournalEntry(RE::GFxMovieView& a_movie, RE::GFxValue& a_root, const RE::GFxValue& a_open)
	{
		RE::GFxValue page;
		RE::GFxValue list;
		RE::GFxValue entries;
		RE::GFxValue original;
		if (!a_movie.GetVariable(&page, "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc") || !page.IsObject() ||
			!page.GetMember("CategoryList", &list) || !list.IsObject() ||
			!list.GetMember("entryList", &entries) || !entries.IsArray())
			return false;
		if (page.GetMember(originalPress, &original) && original.IsObject())
			return true;
		if (!page.GetMember("onCategoryButtonPress", &original) || !original.IsObject())
			return false;
		const auto count = entries.GetArraySize();
		if (count < 7 || count > 9)
			return false;
		std::vector<std::string> labels;
		for (std::uint32_t i = 0; i < count; ++i) {
			RE::GFxValue entry;
			RE::GFxValue label;
			if (!entries.GetElement(i, &entry) || !entry.IsObject() ||
				!entry.GetMember("text", &label) || !label.IsString())
				return false;
			labels.emplace_back(label.GetString());
		}
		std::vector<std::string_view> views(labels.begin(), labels.end());
		if (!IsVanillaJournalCatalog(views))
			return false;

		RE::GFxValue entry;
		RE::GFxValue wrapper;
		a_movie.CreateObject(&entry);
		auto* handler = new CategoryHandler;
		a_movie.CreateFunction(&wrapper, handler);
		handler->Release();
		if (!entry.IsObject() || !wrapper.IsObject() ||
			!entry.SetMember("text", RE::GFxValue("Mod Configuration")) ||
			!entry.SetMember(marker, RE::GFxValue(true)) ||
			!entry.SetMember("disabled", RE::GFxValue(false)))
			return false;

		// Append only: vanilla save guards and action dispatch use positional indices.
		if (!a_root.SetMember(openMethod, a_open))
			return false;
		if (!page.SetMember(originalPress, original) || !page.SetMember("onCategoryButtonPress", wrapper) ||
			!entries.PushBack(entry) || !list.Invoke("InvalidateData", nullptr, nullptr, 0)) {
			if (entries.GetArraySize() > count)
				entries.RemoveElements(count, 1);
			page.SetMember("onCategoryButtonPress", original);
			page.SetMember(originalPress, RE::GFxValue());
			a_root.SetMember(openMethod, RE::GFxValue());
			return false;
		}
		SKSE::log::info("Attached native MCM entry to vanilla Journal; retained {} original categories", count);
		return true;
	}
}
