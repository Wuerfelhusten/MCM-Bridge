#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

namespace MCMBridge
{
	Result<float> NormalizeSliderValue(float a_value, float a_minimum, float a_maximum, float a_step);
	bool          SliderValuesEqual(float a_left, float a_right, const SliderMetadata& a_metadata);
	bool          ControlValuesEqual(const MCMControl& a_control, const MCMValue& a_left, const MCMValue& a_right);
}
