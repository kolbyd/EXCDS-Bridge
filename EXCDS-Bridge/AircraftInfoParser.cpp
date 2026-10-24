#include "AircraftInfoParser.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace {

std::string TrimUpper(std::string s)
{
	auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return s;
}

std::vector<std::string> Split(const std::string& s, char delim)
{
	std::vector<std::string> parts;
	std::string cur;
	for (char c : s) {
		if (c == delim) {
			parts.push_back(cur);
			cur.clear();
		}
		else {
			cur += c;
		}
	}
	parts.push_back(cur);
	return parts;
}

bool IsWtcatLetter(char c)
{
	return c == 'L' || c == 'M' || c == 'H' || c == 'J';
}

bool IsSingleAlpha(const std::string& s)
{
	return s.size() == 1 && std::isalpha(static_cast<unsigned char>(s[0]));
}

bool LooksLikeIcaoType(const std::string& s)
{
	if (s.size() < 2 || s.size() > 4) {
		return false;
	}
	for (char c : s) {
		if (!std::isalnum(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}

bool EquipBlockHasLetter(const std::string& block, char letter)
{
	return block.find(letter) != std::string::npos;
}

bool IcaoBlockImpliesRnav(const std::string& block)
{
	// GNSS / RNP / RNAV-related ICAO Field 10 letters commonly filed on VATSIM.
	static const char kRnavLetters[] = { 'G', 'R', 'I', 'C', 'D', 'O', 'S' };
	for (char letter : kRnavLetters) {
		if (EquipBlockHasLetter(block, letter)) {
			return true;
		}
	}
	return false;
}

bool IcaoBlockImpliesAdsb(const std::string& surveillance)
{
	// B1/B2 = ADS-B out; U1/U2 = ADS-B out (UAT); also treat E/L with B as ADS-B-ish.
	return surveillance.find('B') != std::string::npos
		|| surveillance.find('U') != std::string::npos
		|| surveillance.find('E') != std::string::npos;
}

char MapFaaCapabilityFlags(char letter, bool& rvsm, bool& rnav, bool& adsb)
{
	rvsm = false;
	rnav = false;
	adsb = false;

	switch (letter) {
	case 'L':
	case 'W':
	case 'Q':
		rvsm = true;
		adsb = true;
		rnav = true;
		return letter;
	case 'E':
	case 'F':
	case 'R':
		adsb = true;
		rnav = true;
		return letter;
	case 'Y':
	case 'C':
	case 'I':
	case 'G':
		rnav = true;
		return letter;
	default:
		return letter;
	}
}

} // namespace

AircraftInfoFormat AircraftInfoParser::DetectFormat(const std::string& aircraftInfo)
{
	const std::string info = TrimUpper(aircraftInfo);
	if (info.empty()) {
		return AircraftInfoFormat::Unknown;
	}

	const auto parts = Split(info, '/');
	if (parts.size() >= 2) {
		const std::string& mid = parts[1];
		// ICAO: TYPE / WTC-EQUIPMENT [/ SURVEILLANCE]
		if (mid.size() >= 2 && IsWtcatLetter(mid[0]) && mid[1] == '-') {
			return AircraftInfoFormat::Icao;
		}
	}

	if (parts.size() == 2) {
		// FAA: TYPE/LETTER
		if (LooksLikeIcaoType(parts[0]) && IsSingleAlpha(parts[1])) {
			return AircraftInfoFormat::Faa;
		}
	}

	if (parts.size() == 3) {
		// FAA: WTC/TYPE/LETTER
		if (IsSingleAlpha(parts[0]) && IsWtcatLetter(parts[0][0])
			&& LooksLikeIcaoType(parts[1]) && IsSingleAlpha(parts[2])) {
			return AircraftInfoFormat::Faa;
		}
	}

	return AircraftInfoFormat::Unknown;
}

ParsedAircraftInfo AircraftInfoParser::Parse(const std::string& aircraftInfo)
{
	ParsedAircraftInfo parsed;
	const std::string info = TrimUpper(aircraftInfo);
	parsed.format = DetectFormat(info);
	if (info.empty()) {
		return parsed;
	}

	const auto parts = Split(info, '/');

	if (parsed.format == AircraftInfoFormat::Icao && parts.size() >= 2) {
		parsed.type = parts[0];
		const std::string& mid = parts[1];
		if (mid.size() >= 2 && IsWtcatLetter(mid[0]) && mid[1] == '-') {
			parsed.filedWtcat = mid[0];
			parsed.icaoEquipBlock = mid.substr(2);
		}
		std::string surveillance;
		if (parts.size() >= 3) {
			surveillance = parts[2];
		}
		parsed.parsedRvsm = EquipBlockHasLetter(parsed.icaoEquipBlock, 'W');
		parsed.parsedRnav = IcaoBlockImpliesRnav(parsed.icaoEquipBlock);
		(void)IcaoBlockImpliesAdsb(surveillance);
		return parsed;
	}

	if (parsed.format == AircraftInfoFormat::Faa) {
		if (parts.size() == 3) {
			parsed.filedWtcat = parts[0][0];
			parsed.type = parts[1];
			parsed.faaEquipLetter = parts[2][0];
		}
		else if (parts.size() == 2) {
			parsed.type = parts[0];
			parsed.faaEquipLetter = parts[1][0];
		}
		bool rvsm = false, rnav = false, adsb = false;
		MapFaaCapabilityFlags(parsed.faaEquipLetter, rvsm, rnav, adsb);
		parsed.parsedRvsm = rvsm;
		parsed.parsedRnav = rnav;
		return parsed;
	}

	// Best-effort type extraction for unknown shapes.
	if (!parts.empty() && LooksLikeIcaoType(parts[0])) {
		parsed.type = parts[0];
	}
	else if (parts.size() >= 2 && LooksLikeIcaoType(parts[1])) {
		parsed.type = parts[1];
		if (IsSingleAlpha(parts[0]) && IsWtcatLetter(parts[0][0])) {
			parsed.filedWtcat = parts[0][0];
		}
	}

	return parsed;
}

std::string AircraftInfoParser::CollapseEquipLetter(char faaLetter)
{
	switch (faaLetter) {
	case 'Q':
	case 'W':
	case 'L':
		return "W";
	case 'R':
		return "R";
	case 'G':
	case 'Y':
	case 'C':
	case 'I':
		return "G";
	case 'E':
	case 'F':
		return "E";
	case 'A':
	case 'T':
		return "S";
	default:
		return "N";
	}
}

std::string AircraftInfoParser::CollapseIcaoEquip(const std::string& equipBlock)
{
	const bool hasW = EquipBlockHasLetter(equipBlock, 'W');
	const bool hasR = EquipBlockHasLetter(equipBlock, 'R');
	const bool hasG = EquipBlockHasLetter(equipBlock, 'G');

	if (hasW) {
		return "W";
	}
	if (hasR) {
		return "R";
	}
	if (hasG) {
		return "G";
	}
	if (IcaoBlockImpliesRnav(equipBlock)) {
		return "G";
	}
	if (!equipBlock.empty()) {
		return "S";
	}
	return "N";
}

char AircraftInfoParser::LookupWtcatByType(const std::string& icaoType)
{
	static const std::unordered_map<std::string, char> kMap = {
		// Light
		{ "C152", 'L' }, { "C172", 'L' }, { "C182", 'L' }, { "C208", 'L' },
		{ "PA28", 'L' }, { "PA32", 'L' }, { "PA44", 'L' }, { "BE20", 'L' },
		{ "BE35", 'L' }, { "BE36", 'L' }, { "BE58", 'L' }, { "DA40", 'L' },
		{ "DA42", 'L' }, { "SR20", 'L' }, { "SR22", 'L' }, { "PC12", 'L' },
		{ "TBM8", 'L' }, { "TBM9", 'L' }, { "E50P", 'L' }, { "E55P", 'L' },
		{ "C25A", 'L' }, { "C25B", 'L' }, { "C25C", 'L' }, { "C501", 'L' },
		{ "C510", 'L' }, { "C525", 'L' }, { "C550", 'L' }, { "C560", 'L' },
		{ "C56X", 'L' }, { "C680", 'L' }, { "C68A", 'L' }, { "CL30", 'L' },
		{ "CL35", 'L' }, { "CL60", 'L' }, { "GLF4", 'L' }, { "GLF5", 'L' },
		{ "GLF6", 'L' }, { "GLEX", 'L' }, { "GL5T", 'L' }, { "GL7T", 'L' },
		{ "FA50", 'L' }, { "FA7X", 'L' }, { "FA8X", 'L' }, { "H25B", 'L' },
		{ "LJ35", 'L' }, { "LJ45", 'L' }, { "LJ60", 'L' }, { "AS50", 'L' },
		{ "B06", 'L' }, { "B407", 'L' }, { "EC35", 'L' }, { "R44", 'L' },
		{ "DH8A", 'L' }, { "DH8B", 'L' }, { "DH8C", 'L' }, { "DH8D", 'L' },
		{ "AT43", 'L' }, { "AT45", 'L' }, { "AT46", 'L' }, { "AT72", 'L' },
		{ "AT73", 'L' }, { "AT75", 'L' }, { "AT76", 'L' }, { "SF34", 'L' },
		{ "E110", 'L' }, { "E120", 'L' }, { "BE99", 'L' }, { "BE9L", 'L' },
		// Medium
		{ "A318", 'M' }, { "A319", 'M' }, { "A320", 'M' }, { "A20N", 'M' },
		{ "A321", 'M' }, { "A21N", 'M' }, { "A19N", 'M' },
		{ "B712", 'M' }, { "B717", 'M' }, { "B721", 'M' }, { "B722", 'M' },
		{ "B732", 'M' }, { "B733", 'M' }, { "B734", 'M' }, { "B735", 'M' },
		{ "B736", 'M' }, { "B737", 'M' }, { "B738", 'M' }, { "B739", 'M' },
		{ "B37M", 'M' }, { "B38M", 'M' }, { "B39M", 'M' }, { "B3XM", 'M' },
		{ "B752", 'M' }, { "B753", 'M' },
		{ "E135", 'M' }, { "E145", 'M' }, { "E170", 'M' }, { "E75L", 'M' },
		{ "E75S", 'M' }, { "E190", 'M' }, { "E195", 'M' }, { "E290", 'M' },
		{ "E295", 'M' }, { "CRJ1", 'M' }, { "CRJ2", 'M' }, { "CRJ7", 'M' },
		{ "CRJ9", 'M' }, { "CRJX", 'M' }, { "BCS1", 'M' }, { "BCS3", 'M' },
		{ "MD80", 'M' }, { "MD81", 'M' }, { "MD82", 'M' }, { "MD83", 'M' },
		{ "MD87", 'M' }, { "MD88", 'M' }, { "MD90", 'M' }, { "B462", 'M' },
		{ "RJ1H", 'M' }, { "RJ85", 'M' }, { "BAE1", 'M' }, { "SU95", 'M' },
		{ "T134", 'M' }, { "T154", 'M' }, { "YK40", 'M' }, { "A306", 'M' },
		{ "A30B", 'M' }, { "A310", 'M' }, { "B762", 'M' }, { "B763", 'M' },
		{ "B764", 'M' },
		// Heavy
		{ "A332", 'H' }, { "A333", 'H' }, { "A338", 'H' }, { "A339", 'H' },
		{ "A342", 'H' }, { "A343", 'H' }, { "A345", 'H' }, { "A346", 'H' },
		{ "A359", 'H' }, { "A35K", 'H' },
		{ "B741", 'H' }, { "B742", 'H' }, { "B743", 'H' }, { "B744", 'H' },
		{ "B748", 'H' }, { "B74S", 'H' },
		{ "B772", 'H' }, { "B773", 'H' }, { "B77L", 'H' }, { "B77W", 'H' },
		{ "B788", 'H' }, { "B789", 'H' }, { "B78X", 'H' },
		{ "MD11", 'H' }, { "DC10", 'H' }, { "IL96", 'H' }, { "A124", 'H' },
		{ "C17", 'H' }, { "K35R", 'H' }, { "KC10", 'H' },
		// Super
		{ "A388", 'J' }, { "A225", 'J' },
	};

	const std::string key = TrimUpper(icaoType);
	const auto it = kMap.find(key);
	if (it != kMap.end()) {
		return it->second;
	}
	return '?';
}

ResolvedAircraftCapabilities AircraftInfoParser::Resolve(
	const std::string& aircraftInfo,
	const std::string& fpType,
	char esWtcat,
	char esCapability,
	bool esIsRvsm)
{
	ResolvedAircraftCapabilities out;
	const ParsedAircraftInfo parsed = Parse(aircraftInfo);

	out.type = !fpType.empty() ? TrimUpper(fpType) : parsed.type;
	// EuroScope sometimes returns the full aircraft info as FP type.
	if (out.type.find('/') != std::string::npos) {
		if (!parsed.type.empty()) {
			out.type = parsed.type;
		}
		else {
			out.type = Split(out.type, '/')[0];
		}
	}
	if (out.type.empty()) {
		out.type = parsed.type;
	}

	// Prefer EuroScope capability when it resolved a letter; otherwise parse filed string.
	const bool esCapabilityKnown = esCapability != '\0' && esCapability != '?';
	if (esCapabilityKnown) {
		out.equip = CollapseEquipLetter(esCapability);
		bool rvsm = false, rnav = false, adsb = false;
		MapFaaCapabilityFlags(esCapability, rvsm, rnav, adsb);
		out.rvsm = rvsm;
		out.rnav = rnav;
		out.adsb = adsb;
	}
	else if (parsed.format == AircraftInfoFormat::Faa && parsed.faaEquipLetter != '\0') {
		out.equip = CollapseEquipLetter(parsed.faaEquipLetter);
		bool rvsm = false, rnav = false, adsb = false;
		MapFaaCapabilityFlags(parsed.faaEquipLetter, rvsm, rnav, adsb);
		out.rvsm = rvsm;
		out.rnav = rnav;
		out.adsb = adsb;
	}
	else if (parsed.format == AircraftInfoFormat::Icao) {
		out.equip = CollapseIcaoEquip(parsed.icaoEquipBlock);
		out.rvsm = parsed.parsedRvsm;
		out.rnav = parsed.parsedRnav;
		out.adsb = out.rnav || out.rvsm;
	}
	else {
		out.equip = "N";
	}

	if (esIsRvsm || parsed.parsedRvsm) {
		out.rvsm = true;
	}
	if (parsed.parsedRnav) {
		out.rnav = true;
	}

	if (out.equip.empty()) {
		out.equip = "N";
	}

	// WTC: filed → ES → type map → ?
	if (IsWtcatLetter(parsed.filedWtcat)) {
		out.wtcat = parsed.filedWtcat;
	}
	else if (IsWtcatLetter(esWtcat)) {
		out.wtcat = esWtcat;
	}
	else {
		const char mapped = LookupWtcatByType(out.type);
		out.wtcat = mapped;
	}

	return out;
}

std::string AircraftInfoParser::WtcatToRadarSymbol(char wtcat)
{
	switch (wtcat) {
	case '?':
		return "?";
	case 'L':
		return "-";
	case 'H':
		return "+";
	case 'J':
		return "$";
	case 'M':
	default:
		return "";
	}
}
