#include "MCMBridge/Core/Color.h"

#include <algorithm>
#include <cmath>

namespace
{
	float Component(std::uint32_t a_color, std::uint32_t a_shift)
	{
		return static_cast<float>((a_color >> a_shift) & 0xFFU) / 255.0F;
	}

	std::uint32_t Byte(float a_component)
	{
		return static_cast<std::uint32_t>(std::lround(std::clamp(a_component, 0.0F, 1.0F) * 255.0F));
	}
}

namespace MCMBridge
{
	std::array<float, 4> UnpackARGB(std::uint32_t a_color)
	{
		return { Component(a_color, 16), Component(a_color, 8), Component(a_color, 0), Component(a_color, 24) };
	}

	std::uint32_t PackARGB(const std::array<float, 4>& a_color)
	{
		return (Byte(a_color[3]) << 24) | (Byte(a_color[0]) << 16) | (Byte(a_color[1]) << 8) | Byte(a_color[2]);
	}
}
