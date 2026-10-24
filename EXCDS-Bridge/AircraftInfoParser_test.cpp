/**
 * Standalone fixture runner for AircraftInfoParser.
 * Build (from EXCDS-Bridge folder):
 *   cl /EHsc /std:c++17 /I. EXCDS-Bridge\AircraftInfoParser.cpp EXCDS-Bridge\AircraftInfoParser_test.cpp /Fe:AircraftInfoParser_test.exe
 * Or with g++:
 *   g++ -std=c++17 -I. EXCDS-Bridge/AircraftInfoParser.cpp EXCDS-Bridge/AircraftInfoParser_test.cpp -o AircraftInfoParser_test
 */

#include "AircraftInfoParser.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int gFailures = 0;

void Expect(bool cond, const std::string& msg)
{
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		++gFailures;
	}
	else {
		std::cout << "ok: " << msg << "\n";
	}
}

void ExpectEq(const std::string& actual, const std::string& expected, const std::string& msg)
{
	Expect(actual == expected, msg + " (got \"" + actual + "\", expected \"" + expected + "\")");
}

void ExpectChar(char actual, char expected, const std::string& msg)
{
	Expect(actual == expected, msg + " (got '" + std::string(1, actual) + "', expected '" + std::string(1, expected) + "')");
}

} // namespace

int main()
{
	// Format detection
	Expect(AircraftInfoParser::DetectFormat("H/B738/L") == AircraftInfoFormat::Faa, "FAA H/B738/L");
	Expect(AircraftInfoParser::DetectFormat("B738/W") == AircraftInfoFormat::Faa, "FAA B738/W");
	Expect(AircraftInfoParser::DetectFormat("B738/M-SDE2E3FGHIRWY/LB1") == AircraftInfoFormat::Icao, "ICAO B738/M-...");
	Expect(AircraftInfoParser::DetectFormat("A388/J-SDE1E2E3FGHIJ1RWY/LB1E1") == AircraftInfoFormat::Icao, "ICAO A388/J-...");

	// FAA parse + resolve with good ES values
	{
		auto r = AircraftInfoParser::Resolve("H/B738/L", "B738", 'H', 'L', true);
		ExpectEq(r.equip, "W", "FAA L collapses to W");
		ExpectChar(r.wtcat, 'H', "FAA uses filed WTC");
		Expect(r.rvsm, "FAA L is RVSM");
		Expect(r.rnav, "FAA L is RNAV");
	}

	// ICAO parse when ES capability is unknown
	{
		auto r = AircraftInfoParser::Resolve(
			"B738/M-SDE2E3FGHIRWY/LB1", "B738", '?', '?', false);
		ExpectEq(r.equip, "W", "ICAO with W collapses to W");
		ExpectChar(r.wtcat, 'M', "ICAO filed WTC when ES is ?");
		Expect(r.rvsm, "ICAO W implies RVSM");
		Expect(r.rnav, "ICAO G/R implies RNAV");
	}

	// Always-? path: ES WTC ?, type-only string → type map
	{
		auto r = AircraftInfoParser::Resolve("B738", "B738", '?', '?', false);
		ExpectChar(r.wtcat, 'M', "type map B738→M when ES and filed WTC missing");
		ExpectEq(r.equip, "N", "type-only has no equip");
	}

	{
		auto r = AircraftInfoParser::Resolve("B738/L", "B738", '?', '?', false);
		ExpectChar(r.wtcat, 'M', "FAA without WTC prefix still maps B738→M");
		ExpectEq(r.equip, "W", "FAA /L collapses to W");
		Expect(r.rvsm, "FAA /L is RVSM");
	}

	{
		auto r = AircraftInfoParser::Resolve("A388/J-SDE1FGHIJ1RWY/LB1", "A388", '?', '?', false);
		ExpectChar(r.wtcat, 'J', "ICAO filed J for A388");
		ExpectEq(r.equip, "W", "A388 ICAO with W → equip W");
	}

	{
		auto r = AircraftInfoParser::Resolve("", "B77W", '?', '?', false);
		ExpectChar(r.wtcat, 'H', "empty info, type map B77W→H");
	}

	{
		auto r = AircraftInfoParser::Resolve("", "B738/M-SDE2E3FGHIRWY/LB1", '?', '?', false);
		ExpectChar(r.wtcat, 'M', "FP type that is full ICAO string still maps WTC");
		ExpectEq(r.type, "B738", "strips type from full ICAO FP type string");
	}

	{
		auto r = AircraftInfoParser::Resolve("ZZZZ", "ZZZZ", '?', '?', false);
		ExpectChar(r.wtcat, '?', "unknown type stays ?");
	}

	// Filed WTC wins over ES when both present
	{
		auto r = AircraftInfoParser::Resolve("H/B738/L", "B738", 'M', 'L', true);
		ExpectChar(r.wtcat, 'H', "filed WTC preferred over ES M");
	}

	// ES WTC used only when filed WTC is missing
	{
		auto r = AircraftInfoParser::Resolve("B738/L", "B738", 'H', 'L', true);
		ExpectChar(r.wtcat, 'H', "ES WTC used when FAA has no WTC prefix");
	}

	ExpectEq(AircraftInfoParser::WtcatToRadarSymbol('L'), "-", "radar L");
	ExpectEq(AircraftInfoParser::WtcatToRadarSymbol('M'), "", "radar M empty");
	ExpectEq(AircraftInfoParser::WtcatToRadarSymbol('H'), "+", "radar H");
	ExpectEq(AircraftInfoParser::WtcatToRadarSymbol('J'), "$", "radar J");
	ExpectEq(AircraftInfoParser::WtcatToRadarSymbol('?'), "?", "radar ?");

	if (gFailures) {
		std::cerr << gFailures << " failure(s)\n";
		return EXIT_FAILURE;
	}
	std::cout << "All fixtures passed\n";
	return EXIT_SUCCESS;
}
