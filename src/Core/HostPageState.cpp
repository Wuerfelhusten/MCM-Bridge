#include "MCMBridge/Core/HostPageState.h"

#include <bit>
#include <cmath>

namespace
{
	bool IsComplete(const MCMBridge::ClassicPageBuffers& a_buffers)
	{
		const auto count = a_buffers.optionFlags.size();
		return a_buffers.labels.size() == count && a_buffers.stringValues.size() == count && a_buffers.numericValues.size() == count &&
		       (a_buffers.stateNames.empty() || a_buffers.stateNames.size() == count);
	}

	bool SameStructure(const MCMBridge::ClassicPageBuffers& a_left, const MCMBridge::ClassicPageBuffers& a_right)
	{
		if (a_left.optionFlags.size() != a_right.optionFlags.size() || a_left.labels != a_right.labels || a_left.stateNames != a_right.stateNames)
			return false;
		for (std::size_t index = 0; index < a_left.optionFlags.size(); ++index)
			if ((a_left.optionFlags[index] & 0xFF) != (a_right.optionFlags[index] & 0xFF))
				return false;
		return true;
	}
}

namespace MCMBridge
{
	void HostPageState::Reconcile(std::string a_page, std::int32_t a_index, ClassicPageBuffers a_buffers)
	{
		if (!IsComplete(a_buffers)) {
			Invalidate();
			return;
		}
		if (!current || page != a_page || index != a_index || !SameStructure(baseline, a_buffers)) {
			page = std::move(a_page);
			index = a_index;
			baseline = std::move(a_buffers);
			observed = baseline;
			current = true;
			++structureRevision;
			++valueRevision;
			return;
		}
		bool changed = false;
		for (std::size_t slot = 0; slot < a_buffers.optionFlags.size(); ++slot) {
			const auto reconcile = [&]<class T>(const T& a_before, const T& a_after, T& a_observed) {
				if (a_before != a_after && a_observed != a_after) {
					a_observed = a_after;
					changed = true;
				}
			};
			reconcile(baseline.optionFlags[slot], a_buffers.optionFlags[slot], observed.optionFlags[slot]);
			reconcile(baseline.numericValues[slot], a_buffers.numericValues[slot], observed.numericValues[slot]);
			reconcile(baseline.stringValues[slot], a_buffers.stringValues[slot], observed.stringValues[slot]);
		}
		baseline = std::move(a_buffers);
		if (changed)
			++valueRevision;
	}

	bool HostPageState::Apply(const HostControlChanges& a_changes)
	{
		if (a_changes.resetRequested || a_changes.malformed) {
			Invalidate();
			return false;
		}
		if (!current)
			return false;
		for (const auto& change : a_changes.controls) {
			if (change.index < 0 || static_cast<std::size_t>(change.index) >= observed.optionFlags.size() || (change.number && !std::isfinite(*change.number))) {
				Invalidate();
				return false;
			}
		}
		bool changed = false;
		for (const auto& change : a_changes.controls) {
			const auto slot = static_cast<std::size_t>(change.index);
			const auto assign = [&]<class T>(T& a_target, const T& a_value) {
				if (a_target != a_value) {
					a_target = a_value;
					changed = true;
				}
			};
			if (change.flags) {
				const auto encoded = (std::bit_cast<std::uint32_t>(observed.optionFlags[slot]) & 0xFFU) | (static_cast<std::uint32_t>(*change.flags) << 8U);
				assign(observed.optionFlags[slot], std::bit_cast<std::int32_t>(encoded));
			}
			if (change.number)
				assign(observed.numericValues[slot], *change.number);
			if (change.text)
				assign(observed.stringValues[slot], *change.text);
		}
		if (changed)
			++valueRevision;
		return true;
	}

	const ClassicPageBuffers* HostPageState::Get(std::string_view a_page, std::int32_t a_index) const
	{
		return current && page == a_page && index == a_index ? &observed : nullptr;
	}
}
