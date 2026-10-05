#include "MCMBridge/Papyrus/MCMScript.h"

#include <format>
#include <nlohmann/json.hpp>

namespace MCMBridge
{
	std::optional<std::int32_t> MCMScript::ReadOptionVariable(std::string_view a_name, std::int32_t a_pageIndex) const
	{
		if (a_name.empty() || a_pageIndex < 0 || !IsPageReady(a_pageIndex))
			return std::nullopt;
		const auto value = ReadInteger(a_name);
		const auto flags = ReadArray("_optionFlagsBuf");
		const auto offset = (static_cast<std::int64_t>(a_pageIndex) + 1) * 256;
		if (!value || !flags || *value < offset || *value >= offset + 128)
			return std::nullopt;
		const auto index = static_cast<std::int32_t>(*value - offset);
		return static_cast<std::size_t>(index) < flags->size() ? std::optional{ index } : std::nullopt;
	}

	Result<MCMPage> MCMScript::ReadPage(const ClassicPageContext& a_context) const
	{
		ClassicPageBuffers buffers;
		auto               flags = ReadArray("_optionFlagsBuf");
		auto               labels = ReadArray("_textBuf");
		auto               strings = ReadArray("_strValueBuf");
		auto               numbers = ReadArray("_numValueBuf");
		auto               states = ReadArray("_stateOptionMap");
		if (!flags || !labels || !strings || !numbers) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "SkyUI option buffers are unavailable" });
		}

		const auto count = (std::min)({ flags->size(), labels->size(), strings->size(), numbers->size() });
		buffers.optionFlags.reserve(count);
		buffers.labels.reserve(count);
		buffers.stringValues.reserve(count);
		buffers.numericValues.reserve(count);
		buffers.stateNames.reserve(count);
		for (std::uint32_t index = 0; index < count; ++index) {
			const auto& flag = (*flags)[index];
			const auto& label = (*labels)[index];
			const auto& stringValue = (*strings)[index];
			const auto& number = (*numbers)[index];
			buffers.optionFlags.push_back(flag.IsInt() ? flag.GetSInt() : 0);
			buffers.labels.push_back(label.IsString() ? std::string(label.GetString()) : std::string{});
			buffers.stringValues.push_back(stringValue.IsString() ? std::string(stringValue.GetString()) : std::string{});
			if (number.IsFloat()) {
				buffers.numericValues.push_back(number.GetFloat());
			} else if (number.IsInt()) {
				buffers.numericValues.push_back(static_cast<float>(number.GetSInt()));
			} else {
				buffers.numericValues.push_back(0.0F);
			}
			if (states && index < states->size() && (*states)[index].IsString()) {
				buffers.stateNames.emplace_back((*states)[index].GetString());
			} else {
				buffers.stateNames.emplace_back();
			}
		}
		hostObservation->pageState.Reconcile(a_context.pageName, a_context.pageIndex, buffers);
		if (hostObservation->pendingPageChanges) {
			if (!hostObservation->pageState.Apply(hostObservation->last.changes))
				hostObservation->last.changes.malformed = true;
			hostObservation->pendingPageChanges = false;
		}
		const auto* observed = hostObservation->pageState.Get(a_context.pageName, a_context.pageIndex);
		auto        result = ParseClassicPage(a_context, observed ? *observed : buffers);
		if (spdlog::should_log(spdlog::level::debug))
			SKSE::log::debug(
				"MCM page read: mod={} script={} requested={} index={} current_page={} current_page_num={} state={} live_pages={} controls={} error={}",
				a_context.modID, a_context.scriptName, nlohmann::json(a_context.pageName).dump(), a_context.pageIndex,
				nlohmann::json(ReadScalarString("_currentPage").value_or("")).dump(),
				ReadInteger("_currentPageNum").value_or(-1), ReadInteger("_state").value_or(-1),
				nlohmann::json(ReadPages()).dump(), result ? result->controls.size() : 0,
				result ? "" : result.error().message);
		return result;
	}

	const RE::BSScript::Variable* MCMScript::FindVariable(std::string_view a_name) const
	{
		if (!script) {
			return nullptr;
		}
		const RE::BSFixedString fixedName(a_name);
		const auto*             value = script->GetVariable(fixedName);
		if (!value) {
			value = script->GetProperty(fixedName);
		}
		if (!value) {
			value = script->GetVariable(RE::BSFixedString(std::format("::{}_var", a_name)));
		}
		return value;
	}

	RE::BSTSmartPointer<RE::BSScript::Array> MCMScript::ReadArray(std::string_view a_name) const
	{
		const auto* value = FindVariable(a_name);
		return value && value->IsArray() ? value->GetArray() : RE::BSTSmartPointer<RE::BSScript::Array>();
	}

	std::optional<float> MCMScript::ReadNumber(std::string_view a_name, std::size_t a_index) const
	{
		auto values = ReadArray(a_name);
		if (!values || a_index >= values->size()) {
			return std::nullopt;
		}
		const auto& value = (*values)[static_cast<std::uint32_t>(a_index)];
		if (value.IsFloat()) {
			return value.GetFloat();
		}
		if (value.IsInt()) {
			return static_cast<float>(value.GetSInt());
		}
		if (value.IsBool()) {
			return value.GetBool() ? 1.0F : 0.0F;
		}
		return std::nullopt;
	}

	std::optional<std::string> MCMScript::ReadString(std::string_view a_name, std::size_t a_index) const
	{
		auto values = ReadArray(a_name);
		if (!values || a_index >= values->size()) {
			return std::nullopt;
		}
		const auto& value = (*values)[static_cast<std::uint32_t>(a_index)];
		return value.IsString() ? std::optional<std::string>(std::string(value.GetString())) : std::nullopt;
	}

	std::optional<std::string> MCMScript::ReadScalarString(std::string_view a_name) const
	{
		const auto* value = FindVariable(a_name);
		return value && value->IsString() ? std::optional<std::string>(std::string(value->GetString())) : std::nullopt;
	}

	std::optional<std::int32_t> MCMScript::ReadInteger(std::string_view a_name) const
	{
		const auto* value = FindVariable(a_name);
		return value && value->IsInt() ? std::optional<std::int32_t>(value->GetSInt()) : std::nullopt;
	}
}
