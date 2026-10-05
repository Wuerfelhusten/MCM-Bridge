#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include "Plugin.h"

#ifdef GetObject
#	undef GetObject
#endif

#ifdef min
#	undef min
#endif

#ifdef max
#	undef max
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "MCMBridge/Plugin/Plugin.h"

using namespace std::literals;
