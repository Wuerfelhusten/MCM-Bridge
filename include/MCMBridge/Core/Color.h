#pragma once

#include <array>
#include <cstdint>

namespace MCMBridge
{
	std::array<float, 4> UnpackARGB(std::uint32_t a_color);
	std::uint32_t        PackARGB(const std::array<float, 4>& a_color);
}
