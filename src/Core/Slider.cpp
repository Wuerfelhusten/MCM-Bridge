#include "MCMBridge/Core/Slider.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MCMBridge
{
	Result<float> NormalizeSliderValue(float a_value, float a_minimum, float a_maximum, float a_step)
	{
		if (!std::isfinite(a_value) || !std::isfinite(a_minimum) || !std::isfinite(a_maximum) ||
			!std::isfinite(a_step) || a_minimum > a_maximum || a_step <= 0.0F) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Invalid slider metadata" });
		}

		const double clamped = std::clamp(a_value, a_minimum, a_maximum);
		const double steps = std::round((clamped - a_minimum) / a_step);
		const double offset = steps * a_step;
		const double snapped = std::clamp(a_minimum + offset, static_cast<double>(a_minimum), static_cast<double>(a_maximum));
		// Cancellation near zero inherits uncertainty from the float grid operands,
		// not the much smaller ULP of the result. Preserve an already aligned input.
		const double scale = std::max({ std::abs(static_cast<double>(a_minimum)), std::abs(offset), std::abs(clamped) });
		const double tolerance = std::min(4.0 * std::numeric_limits<float>::epsilon() * scale, static_cast<double>(a_step) / 16.0);
		return static_cast<float>(std::abs(snapped - clamped) <= tolerance ? clamped : snapped);
	}

	bool SliderValuesEqual(float a_left, float a_right, const SliderMetadata& a_metadata)
	{
		if (!NormalizeSliderValue(a_left, a_metadata.minimum, a_metadata.maximum, a_metadata.step) ||
			!NormalizeSliderValue(a_right, a_metadata.minimum, a_metadata.maximum, a_metadata.step))
			return false;
		if (a_left == a_right)
			return true;
		const auto ulp = [](float a_value) {
			const float magnitude = std::abs(a_value);
			return static_cast<double>(magnitude) - std::nextafter(magnitude, 0.0F);
		};
		// Never let representation noise hide a meaningful fraction of a slider step.
		const double tolerance = std::min(4.0 * std::max(ulp(a_left), ulp(a_right)), static_cast<double>(a_metadata.step) / 16.0);
		return std::abs(static_cast<double>(a_left) - a_right) <= tolerance;
	}

	bool ControlValuesEqual(const MCMControl& a_control, const MCMValue& a_left, const MCMValue& a_right)
	{
		if (a_control.type != MCMControlType::kSlider)
			return a_left == a_right;
		const auto* left = std::get_if<float>(&a_left);
		const auto* right = std::get_if<float>(&a_right);
		if (!left || !right)
			return false;
		// Fresh confirmation buffers need no dialog read for exact finite equality.
		if (!a_control.slider)
			return std::isfinite(*left) && *left == *right;
		return SliderValuesEqual(*left, *right, *a_control.slider);
	}
}
