#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
	struct Reader
	{
		std::string_view bytes;
		std::string_view Take(std::size_t a_size)
		{
			if (a_size > bytes.size())
				throw std::out_of_range("Truncated plugin field");
			const auto result = bytes.substr(0, a_size);
			bytes.remove_prefix(a_size);
			return result;
		}
		std::uint32_t Number(std::size_t a_width)
		{
			REQUIRE(a_width <= 4);
			const auto    data = Take(a_width);
			std::uint32_t result{};
			for (std::size_t index = 0; index < a_width; ++index)
				result |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[index])) << (index * 8);
			return result;
		}
		std::string_view String() { return Take(Number(2)); }
	};

	std::map<std::string_view, std::string_view> Fields(std::string_view a_data)
	{
		Reader                                       input{ a_data };
		std::map<std::string_view, std::string_view> fields;
		while (!input.bytes.empty()) {
			const auto tag = input.Take(4);
			const auto value = input.Take(input.Number(2));
			REQUIRE(fields.emplace(tag, value).second);
		}
		return fields;
	}

	std::string Load(std::string_view a_name)
	{
		std::ifstream input(std::string(MCM_BRIDGE_BOOTSTRAP_DIR) + "/" + std::string(a_name), std::ios::binary);
		REQUIRE(input.good());
		return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	}
}

TEST_CASE("Standalone bootstrap preserves the manager form and player alias contracts")
{
	const auto bytes = Load("SkyUI_SE.esp");
	Reader     input{ bytes };
	REQUIRE(input.Take(4) == "TES4");
	const auto headerSize = input.Number(4);
	REQUIRE(input.Number(4) == 0);
	REQUIRE(input.Number(4) == 0);
	input.Take(8);
	const auto header = Fields(input.Take(headerSize));
	REQUIRE(header.at("MAST") == std::string_view("Skyrim.esm\0", 11));
	Reader hedr{ header.at("HEDR") };
	hedr.Take(4);
	REQUIRE(hedr.Number(4) == 2);
	REQUIRE(hedr.Number(4) == 0x803);
	REQUIRE(input.Take(4) == "GRUP");
	const auto groupSize = input.Number(4);
	REQUIRE(groupSize == input.bytes.size() + 8);
	REQUIRE(input.Take(4) == "QUST");
	REQUIRE(input.Number(4) == 0);
	input.Take(8);
	REQUIRE(input.Take(4) == "QUST");
	const auto questSize = input.Number(4);
	REQUIRE(input.Number(4) == 0);
	REQUIRE(input.Number(4) == 0x01000802);
	input.Take(4);
	REQUIRE(input.Number(2) == 44);
	input.Take(2);
	const auto quest = Fields(input.Take(questSize));
	REQUIRE(input.bytes.empty());
	REQUIRE(quest.at("EDID") == std::string_view("SKI_ConfigManagerInstance\0", 26));
	REQUIRE_FALSE(quest.contains("INDX"));
	REQUIRE(Reader{ quest.at("DNAM") }.Number(2) == 0x111);
	REQUIRE(quest.at("DNAM").size() == 12);
	REQUIRE(Reader{ quest.at("ANAM") }.Number(4) == 1);
	REQUIRE(Reader{ quest.at("ALST") }.Number(4) == 0);
	REQUIRE(Reader{ quest.at("ALFR") }.Number(4) == 0x14);
	REQUIRE(quest.at("ALED").empty());

	Reader vmad{ quest.at("VMAD") };
	REQUIRE(vmad.Number(2) == 5);
	REQUIRE(vmad.Number(2) == 2);
	REQUIRE(vmad.Number(2) == 1);
	REQUIRE(vmad.String() == "SKI_ConfigManager");
	REQUIRE(vmad.Number(1) == 0);
	REQUIRE(vmad.Number(2) == 0);
	REQUIRE(vmad.Number(1) == 2);
	REQUIRE(vmad.Number(2) == 0);
	REQUIRE(vmad.String().empty());
	REQUIRE(vmad.Number(2) == 1);
	REQUIRE(vmad.Number(2) == 0);
	REQUIRE(vmad.Number(2) == 0);
	REQUIRE(vmad.Number(4) == 0x01000802);
	REQUIRE(vmad.Number(2) == 5);
	REQUIRE(vmad.Number(2) == 2);
	REQUIRE(vmad.Number(2) == 1);
	REQUIRE(vmad.String() == "SKI_PlayerLoadGameAlias");
	REQUIRE(vmad.Number(1) == 0);
	REQUIRE(vmad.Number(2) == 0);
	REQUIRE(vmad.bytes.empty());
	const auto sequence = Load("Seq/SkyUI_SE.seq");
	REQUIRE(sequence.size() == 4);
	REQUIRE(Reader{ sequence }.Number(4) == 0x01000802);
}
