#pragma once

#include <string>

enum class AircraftInfoFormat
{
	Unknown,
	Faa,
	Icao
};

struct ParsedAircraftInfo
{
	AircraftInfoFormat format = AircraftInfoFormat::Unknown;
	std::string type;
	char filedWtcat = '?';
	char faaEquipLetter = '\0';
	std::string icaoEquipBlock;
	bool parsedRvsm = false;
	bool parsedRnav = false;
};

struct ResolvedAircraftCapabilities
{
	std::string type;
	std::string equip;
	char wtcat = '?';
	bool rvsm = false;
	bool rnav = false;
	bool adsb = false;
};

class AircraftInfoParser
{
public:
	static ParsedAircraftInfo Parse(const std::string& aircraftInfo);
	static AircraftInfoFormat DetectFormat(const std::string& aircraftInfo);

	/** Collapse FAA capability / parsed ICAO equipment to W/R/G/E/S/N. */
	static std::string CollapseEquipLetter(char faaLetter);
	static std::string CollapseIcaoEquip(const std::string& equipBlock);

	static char LookupWtcatByType(const std::string& icaoType);

	/**
	 * Resolve display fields from EuroScope values + raw aircraft info.
	 * WTC order: filed string → ES L/M/H/J → type map → '?'
	 * Equip/RVSM/RNAV: known ES capability → parsed FAA/ICAO → IsRvsm
	 */
	static ResolvedAircraftCapabilities Resolve(
		const std::string& aircraftInfo,
		const std::string& fpType,
		char esWtcat,
		char esCapability,
		bool esIsRvsm);

	static std::string WtcatToRadarSymbol(char wtcat);
};
