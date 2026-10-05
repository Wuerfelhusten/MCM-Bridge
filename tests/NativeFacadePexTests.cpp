#include "MCMBridge/Core/PapyrusIdentifier.h"
#include "MCMBridge/Core/PexIdentity.h"
#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("Shipped PEX identity rejects altered code and tolerates compiler timestamps")
{
	for (const auto* name : { "SKI_ConfigManager", "SKI_ConfigBase", "MCMBridgeNative", "MCMBridgeRegistry" }) {
		INFO(name);
		std::ifstream input(std::filesystem::path(MCM_BRIDGE_NATIVE_SCRIPT_DIR) / (std::string(name) + ".pex"), std::ios::binary);
		REQUIRE(input.good());
		const std::vector<std::uint8_t> shipped(std::istreambuf_iterator<char>{ input }, {});
		REQUIRE(MCMBridge::MatchesShippedPex(shipped, shipped));
		auto changed = shipped;
		changed[8] ^= 1;
		CHECK(MCMBridge::MatchesShippedPex(changed, shipped));
		changed.back() ^= 1;
		CHECK_FALSE(MCMBridge::MatchesShippedPex(changed, shipped));
		changed = shipped;
		changed[0] ^= 1;
		CHECK_FALSE(MCMBridge::MatchesShippedPex(changed, shipped));
		for (std::size_t size : { std::size_t(0), std::size_t(15), std::size_t(20), shipped.size() - 1 })
			CHECK_FALSE(MCMBridge::MatchesShippedPex(std::span(shipped).first(size), shipped));
		changed = shipped;
		changed[16] = 0xFF;
		changed[17] = 0xFF;
		CHECK_FALSE(MCMBridge::MatchesShippedPex(changed, shipped));
	}
}

namespace
{
	std::string TypeName(std::string a_name)
	{
		for (auto& character : a_name)
			if (character >= 'A' && character <= 'Z')
				character = static_cast<char>(character + ('a' - 'A'));
		return a_name;
	}

	// This reader checks the emitted Skyrim property table, not source declarations.
	// Getter bodies in these facades must be single returns; other bodies fail closed.
	class FacadePex
	{
	public:
		explicit FacadePex(const std::filesystem::path& a_path)
		{
			std::ifstream input(a_path, std::ios::binary);
			if (!input)
				throw std::runtime_error("Missing compiled facade");
			bytes.assign(std::istreambuf_iterator<char>(input), {});
			if (Number(4) != 0xFA57C0DE || Number(1) != 3 || Number(1) != 2 || Number(2) != 1)
				throw std::runtime_error("Expected Skyrim PEX 3.2");
			Skip(8);
			for (int index = 0; index < 3; ++index)
				Text();
			const auto strings = Number(2);
			for (std::uint32_t index = 0; index < strings; ++index)
				table.push_back(Text());
			if (Number(1)) {
				Skip(8);
				const auto functions = Number(2);
				for (std::uint32_t index = 0; index < functions; ++index) {
					Skip(7);
					Skip(2 * Number(2));
				}
			}
			Skip(3 * Number(2));
			if (Number(2) != 1)
				throw std::runtime_error("Expected one facade object");
			name = Reference();
			Skip(4);
			parent = Reference();
			Reference();
			Skip(4);
			Reference();
			const auto variableCount = Number(2);
			for (std::uint32_t index = 0; index < variableCount; ++index) {
				auto variable = Reference();
				auto type = Reference();
				Skip(4);
				variables.emplace(std::move(variable), Variable{ std::move(type), Value() });
			}
			const auto propertyCount = Number(2);
			for (std::uint32_t index = 0; index < propertyCount; ++index) {
				auto property = Reference();
				auto type = Reference();
				Reference();
				Skip(4);
				const auto  flags = Number(1);
				std::string backing;
				if (flags & 4)
					backing = Reference();
				else {
					if (flags & 1)
						Getter();
					if (flags & 2)
						throw std::runtime_error("Unexpected custom facade setter");
				}
				properties.emplace(std::move(property), Property{ std::move(type), flags, std::move(backing) });
			}
		}

		struct Variable
		{
			std::string                 type;
			std::optional<std::int32_t> value;
		};
		struct Property
		{
			std::string   type;
			std::uint32_t flags{};
			std::string   backing;
		};
		std::string                     name;
		std::string                     parent;
		std::map<std::string, Variable> variables;
		std::map<std::string, Property> properties;

	private:
		void Skip(std::size_t a_size)
		{
			if (a_size > bytes.size() - offset)
				throw std::runtime_error("Truncated facade PEX");
			offset += a_size;
		}
		std::uint32_t Number(std::size_t a_size)
		{
			const auto start = offset;
			Skip(a_size);
			std::uint32_t result{};
			for (std::size_t index = start; index < offset; ++index)
				result = (result << 8) | bytes[index];
			return result;
		}
		std::string Text()
		{
			const auto size = Number(2);
			const auto start = offset;
			Skip(size);
			return std::string(bytes.begin() + start, bytes.begin() + offset);
		}
		std::string                 Reference() { return table.at(Number(2)); }
		std::optional<std::int32_t> Value()
		{
			switch (Number(1)) {
			case 0:
				return {};
			case 1:
			case 2:
				Reference();
				return {};
			case 3:
				return std::bit_cast<std::int32_t>(Number(4));
			case 4:
				Skip(4);
				return {};
			case 5:
				Skip(1);
				return {};
			default:
				throw std::runtime_error("Unknown PEX value tag");
			}
		}
		void Getter()
		{
			Skip(9);
			Skip(4 * Number(2));
			Skip(4 * Number(2));
			if (Number(2) != 1 || Number(1) != 26)
				throw std::runtime_error("Expected a single-return facade getter");
			Value();
		}
		std::vector<unsigned char> bytes;
		std::vector<std::string>   table;
		std::size_t                offset{};
	};
}

TEST_CASE("Facade identifiers accept VM casing without accepting different contracts")
{
	using MCMBridge::SamePapyrusIdentifier;
	CHECK(SamePapyrusIdentifier("ski_configmanager", "SKI_ConfigManager"));
	CHECK(SamePapyrusIdentifier("SKI_CONFIGBASE", "SKI_ConfigBase"));
	CHECK(SamePapyrusIdentifier("bridgecalladmissioncontract", "BridgeCallAdmissionContract"));
	CHECK(SamePapyrusIdentifier("bRiDgEaWaIt", "BridgeAwait"));
	CHECK_FALSE(SamePapyrusIdentifier("SKI_ConfigBase", "SKI_ConfigManager"));
	CHECK_FALSE(SamePapyrusIdentifier("BridgeAwait", "BridgeAwaitCall"));
	CHECK_FALSE(SamePapyrusIdentifier("", "BridgeEnterCall"));
}

TEST_CASE("Compiled facade version properties have native-readable backing storage")
{
	for (const auto& [script, property] : std::vector<std::pair<std::string, std::string>>{
			 { "SKI_ConfigBase", "MCMBridgeFacadeVersion" }, { "SKI_ConfigManager", "MCMBridgeManagerVersion" } }) {
		CAPTURE(script, property);
		const FacadePex pex(std::filesystem::path(MCM_BRIDGE_NATIVE_SCRIPT_DIR) / (script + ".pex"));
		CHECK(pex.name == script);
		CHECK(pex.parent == "SKI_QuestBase");
		const auto& version = pex.properties.at(property);
		CHECK(version.type == "Int");
		REQUIRE((version.flags & 5) == 5);
		REQUIRE_FALSE(version.backing.empty());
		const auto& stored = pex.variables.at(version.backing);
		CHECK(stored.type == "Int");
		CHECK(stored.value == 1);
	}
}

TEST_CASE("Compiled facades retain native and Helper field types")
{
	const auto      root = std::filesystem::path(MCM_BRIDGE_NATIVE_SCRIPT_DIR);
	const FacadePex config(root / "SKI_ConfigBase.pex");
	for (const auto& [name, type] : std::vector<std::pair<std::string, std::string>>{
			 { "_optionFlagsBuf", "int[]" }, { "_textBuf", "string[]" },
			 { "_strValueBuf", "string[]" }, { "_numValueBuf", "float[]" },
			 { "_stateOptionMap", "string[]" }, { "_sliderParams", "float[]" },
			 { "_menuParams", "int[]" }, { "_colorParams", "int[]" },
			 { "_currentPage", "string" }, { "_currentPageNum", "int" },
			 { "_cursorPosition", "int" }, { "_cursorFillMode", "int" },
			 { "_activeOption", "int" }, { "_state", "int" } }) {
		CAPTURE(name);
		CHECK(TypeName(config.variables.at(name).type) == type);
	}
	for (const auto& name : { "ModName", "Pages" }) {
		CAPTURE(name);
		const auto& property = config.properties.at(name);
		REQUIRE((property.flags & 7) == 7);
		CHECK(config.variables.at(property.backing).type == property.type);
	}
	const FacadePex manager(root / "SKI_ConfigManager.pex");
	CHECK(TypeName(manager.variables.at("_modConfigs").type) == "ski_configbase[]");
	CHECK(TypeName(manager.variables.at("_modNames").type) == "string[]");
	CHECK(manager.variables.at("_configCount").value == 0);
}
