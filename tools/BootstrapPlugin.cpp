#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
	using Bytes = std::vector<std::uint8_t>;
	constexpr std::uint32_t managerID = 0x01000802;

	void Integer(Bytes& a_data, std::uint64_t a_value, unsigned a_width)
	{
		for (unsigned index = 0; index < a_width; ++index)
			a_data.push_back(static_cast<std::uint8_t>(a_value >> (index * 8)));
	}

	void Append(Bytes& a_data, std::span<const std::uint8_t> a_value)
	{
		a_data.insert(a_data.end(), a_value.begin(), a_value.end());
	}

	void Text(Bytes& a_data, std::string_view a_text)
	{
		a_data.insert(a_data.end(), a_text.begin(), a_text.end());
	}

	void SizedText(Bytes& a_data, std::string_view a_text)
	{
		if (a_text.size() > std::numeric_limits<std::uint16_t>::max())
			throw std::length_error("Plugin string exceeds the field size");
		Integer(a_data, a_text.size(), 2);
		Text(a_data, a_text);
	}

	void Field(Bytes& a_data, std::string_view a_tag, const Bytes& a_value)
	{
		if (a_tag.size() != 4 || a_value.size() > std::numeric_limits<std::uint16_t>::max())
			throw std::length_error("Invalid bootstrap subrecord");
		Text(a_data, a_tag);
		Integer(a_data, a_value.size(), 2);
		Append(a_data, a_value);
	}

	void StringField(Bytes& a_data, std::string_view a_tag, std::string_view a_value)
	{
		Bytes text;
		Text(text, a_value);
		text.push_back(0);
		Field(a_data, a_tag, text);
	}

	void NumberField(Bytes& a_data, std::string_view a_tag, std::uint32_t a_value)
	{
		Bytes value;
		Integer(value, a_value, 4);
		Field(a_data, a_tag, value);
	}

	Bytes Record(std::string_view a_tag, std::uint32_t a_id, std::uint32_t a_flags, const Bytes& a_data)
	{
		Bytes result;
		Text(result, a_tag);
		Integer(result, a_data.size(), 4);
		Integer(result, a_flags, 4);
		Integer(result, a_id, 4);
		Integer(result, 0, 4);
		Integer(result, 44, 2);
		Integer(result, 0, 2);
		Append(result, a_data);
		return result;
	}

	void Script(Bytes& a_data, std::string_view a_name)
	{
		SizedText(a_data, a_name);
		Integer(a_data, 0, 1);
		Integer(a_data, 0, 2);
	}

	Bytes Manager()
	{
		Bytes vmad;
		Integer(vmad, 5, 2);
		Integer(vmad, 2, 2);
		Integer(vmad, 1, 2);
		Script(vmad, "SKI_ConfigManager");
		// QUST fragment data is present to reach the alias block, without stage callbacks.
		Integer(vmad, 2, 1);
		Integer(vmad, 0, 2);
		SizedText(vmad, "");
		Integer(vmad, 1, 2);
		Integer(vmad, 0, 2);
		Integer(vmad, 0, 2);
		Integer(vmad, managerID, 4);
		Integer(vmad, 5, 2);
		Integer(vmad, 2, 2);
		Integer(vmad, 1, 2);
		Script(vmad, "SKI_PlayerLoadGameAlias");

		Bytes quest;
		StringField(quest, "EDID", "SKI_ConfigManagerInstance");
		Field(quest, "VMAD", vmad);
		StringField(quest, "FULL", "MCM Bridge Native Manager");
		Bytes general;
		Integer(general, 0x111, 2);  // Start Game Enabled, Starts Enabled, Run Once.
		Integer(general, 0, 1);
		Integer(general, 255, 1);
		Integer(general, 0, 8);
		Field(quest, "DNAM", general);
		Field(quest, "NEXT", {});
		NumberField(quest, "ANAM", 1);
		NumberField(quest, "ALST", 0);
		StringField(quest, "ALID", "PlayerRef");
		NumberField(quest, "FNAM", 0);
		NumberField(quest, "ALFR", 0x14);
		NumberField(quest, "VTCK", 0);
		Field(quest, "ALED", {});
		return Record("QUST", managerID, 0, quest);
	}

	Bytes Plugin()
	{
		Bytes hedr;
		Integer(hedr, std::bit_cast<std::uint32_t>(1.7F), 4);
		Integer(hedr, 2, 4);  // One record and one group, excluding TES4.
		Integer(hedr, 0x803, 4);
		Bytes header;
		Field(header, "HEDR", hedr);
		StringField(header, "CNAM", "MCM Bridge Contributors");
		StringField(header, "SNAM", "Standalone native MCM bootstrap. Do not install with SkyUI.");
		StringField(header, "MAST", "Skyrim.esm");
		Field(header, "DATA", Bytes(8));
		auto result = Record("TES4", 0, 0, header);
		auto manager = Manager();
		Text(result, "GRUP");
		Integer(result, manager.size() + 24, 4);
		Text(result, "QUST");
		Integer(result, 0, 4);
		Integer(result, 0, 8);
		Append(result, manager);
		return result;
	}

	void Write(const std::filesystem::path& a_path, const Bytes& a_data)
	{
		std::filesystem::create_directories(a_path.parent_path());
		std::ofstream file(a_path, std::ios::binary | std::ios::trunc);
		file.exceptions(std::ios::failbit | std::ios::badbit);
		file.write(reinterpret_cast<const char*>(a_data.data()), static_cast<std::streamsize>(a_data.size()));
		file.close();
	}
}

int main(int a_count, char** a_arguments)
{
	try {
		if (a_count != 2)
			throw std::invalid_argument("Usage: MCMBridgeBootstrap <build-output-directory>");
		const std::filesystem::path output(a_arguments[1]);
		Write(output / "SkyUI_SE.esp", Plugin());
		Bytes sequence;
		Integer(sequence, managerID, 4);
		Write(output / "Seq" / "SkyUI_SE.seq", sequence);
		return 0;
	} catch (const std::exception& a_error) {
		std::cerr << a_error.what() << '\n';
		return 1;
	}
}
