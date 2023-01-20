#include "pch.h"
#include "sio_client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <time.h>

#include "EuroScopePlugIn.h"
#include "CEXCDSBridge.h"

#include "MessageHandler.h"

using namespace sio;

// Aircraft tuple is:
// 0: Callsign
// 1: Departure Airport
// 2: Route
// 3: Arrival Airport
// 4: Speed
// 5: Transponder
// 6: Final altitude
// 7: Coordinated Altitude

std::tuple<std::string, std::string> aircraft;

// Flight strip annotations
// 0 - Reserved for situ
// 6 - Estimate vor
// 7 - Estimate time
// 8 - Reported altitude

/**
* ---------------------------
* Updating methods
* 
* These are for EXCDS to tell us to update aircraft in EuroScope.
* ---------------------------
*/

void MessageHandler::UpdateScratchPad(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string value = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	bool isAssigned = fp.GetControllerAssignedData().SetScratchPadString(value.c_str());
	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateAltitude(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string id = e.get_message()->get_map()["id"]->get_string();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	int cleared = e.get_message()->get_map()["cleared"]->get_int();
	int final = e.get_message()->get_map()["final"]->get_int();
	int coordinated = e.get_message()->get_map()["coordinated"]->get_int();
	// Reported altitude, goes to strip annotations for situ
	std::string reported = e.get_message()->get_map()["reported"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	if (cleared != -1)
		fp.GetControllerAssignedData().SetClearedAltitude(cleared);
	if (final != -1) {
		fp.GetFlightPlanData().SetFinalAltitude(final);
		fp.GetControllerAssignedData().SetFinalAltitude(final);
	}
	if (coordinated != -1)
		fp.InitiateCoordination(fp.GetCoordinatedNextController(), fp.GetNextCopxPointName(), coordinated);
	if (reported != "")
		fp.GetControllerAssignedData().SetFlightStripAnnotation(8, reported.c_str());

	fp.GetFlightPlanData().AmendFlightPlan();

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateSpeed(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string id = e.get_message()->get_map()["id"]->get_string();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	int assignedMach = e.get_message()->get_map()["assignedMach"]->get_int();
	int assignedSpeed = e.get_message()->get_map()["assignedSpeed"]->get_int();
	int filedSpeed = e.get_message()->get_map()["filedSpeed"]->get_int();

	// Unused for now 
	//std::string ifrString = e.get_message()->get_map()["ifrString"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	// Assign either a speed or mach, if provided
	bool isAssigned = false;
	if (assignedMach > 0) {
		isAssigned = fp.GetControllerAssignedData().SetAssignedMach(assignedMach);
	}
	else if (assignedSpeed > 0) {
		isAssigned = fp.GetControllerAssignedData().SetAssignedSpeed(assignedSpeed);
	}

	// Assign the filed speed, if provided
	if (filedSpeed > 0) {
		isAssigned = fp.GetFlightPlanData().SetTrueAirspeed(filedSpeed);
	}

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateFlightPlan(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string flightRules = e.get_message()->get_map()["flight_rules"]->get_string();
	std::string acType = e.get_message()->get_map()["aircraft_type"]->get_string();
	std::string origin = e.get_message()->get_map()["origin"]->get_string();
	std::string destination = e.get_message()->get_map()["destination"]->get_string();
	int altitude = e.get_message()->get_map()["altitude"]->get_int();
	int speed = e.get_message()->get_map()["speed"]->get_int();
	std::string etehours = e.get_message()->get_map()["etehours"]->get_string();
	std::string eteminutes = e.get_message()->get_map()["eteminutes"]->get_string();
	std::string route = e.get_message()->get_map()["route"]->get_string();
	std::string state = e.get_message()->get_map()["state"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	MessageHandler::StatusAssign(state, fp, "");

	// Assign data
	fp.GetFlightPlanData().SetPlanType(flightRules.c_str());
	fp.GetFlightPlanData().SetAircraftInfo(acType.c_str());
	fp.GetFlightPlanData().SetOrigin(origin.c_str());
	fp.GetFlightPlanData().SetDestination(destination.c_str());
	fp.GetFlightPlanData().SetFinalAltitude(altitude);
	fp.GetControllerAssignedData().SetFinalAltitude(altitude);
	fp.GetFlightPlanData().SetTrueAirspeed(speed);
	fp.GetFlightPlanData().SetEnrouteHours(etehours.c_str());
	fp.GetFlightPlanData().SetEnrouteMinutes(eteminutes.c_str());
	fp.GetFlightPlanData().SetRoute(route.c_str());

	fp.GetFlightPlanData().AmendFlightPlan();

	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateRunway(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string id = e.get_message()->get_map()["id"]->get_string();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string newRunway = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	// Is runway code valid?
	if (!regex_match(newRunway, std::regex("^(0?[1-9]|[1-2][0-9]|3[0-6])[LCR]?$")))
	{
		e.put_ack_message(NotModified(response, "Runway code is invalid."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "INVLD_RWY");
		return;
	}
	
	std::string route;
	if (id == "UPDATE_DEPARTURE_RUNWAY")
	{
		route = AddRunwayToRoute(newRunway, fp, true);
	}
	else if (id == "UPDATE_ARRIVAL_RUNWAY")
	{
		route = AddRunwayToRoute(newRunway, fp, false);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown update type."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN_TYP");
		return;
	}

	fp.GetFlightPlanData().AmendFlightPlan();

	// Check if it assigned properly
	bool isAssigned = fp.GetFlightPlanData().SetRoute(route.c_str());
	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateSitu(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string id = e.get_message()->get_map()["id"]->get_string();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	bool isAssigned = false;
	if (id == "GRANT_RELEASE")
	{
		// 1 = FSS / 5 = APP/DEP / 6 = CTR
		if (CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() > 1 && CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() < 5)
		{
			e.put_ack_message(NotModified(response, "Permission denied."));

			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "RREL_NT_PRMTD");
			return;
		}

		isAssigned = fp.GetControllerAssignedData().SetScratchPadString("RREL");
	} else {
		isAssigned = fp.GetControllerAssignedData().SetScratchPadString("RREQ");
	}

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateEstimate(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string time = e.get_message()->get_map()["time"]->get_string();
	std::string vor = e.get_message()->get_map()["vor"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	std::string vorDebug = "Recived VOR string: " + vor;
	OutputDebugString(vorDebug.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	fp.GetControllerAssignedData().SetFlightStripAnnotation(6, vor.c_str());
	fp.GetControllerAssignedData().SetFlightStripAnnotation(7, "");

	fp.GetFlightPlanData().AmendFlightPlan();

	//if (!isAssigned) {
	//	e.put_ack_message(NotModified(response, "Unknown reason."));

	//	CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	//	return;
	//}

	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateAircraftStatus(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string status = e.get_message()->get_map()["status"]->get_string();
	std::string departureTime = e.get_message()->get_map()["departure_time"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	bool success = MessageHandler::StatusAssign(status, fp, departureTime);

	fp.GetFlightPlanData().AmendFlightPlan();

	if (success) {
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	}
}

void MessageHandler::UpdateDepartureInstructions(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	int altitude = e.get_message()->get_map()["altitude"]->get_int();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	bool success = false;

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	if (altitude > 0) {
		fp.GetControllerAssignedData().SetClearedAltitude(altitude);
	}

	fp.GetFlightPlanData().AmendFlightPlan();

	if (success) {
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	}
}

void MessageHandler::UpdateArrivalInstructions(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string runway = e.get_message()->get_map()["runway"]->get_string();
	std::string requestedApproach = e.get_message()->get_map()["approach"]->get_string();
	std::string directTo = e.get_message()->get_map()["direct_to"]->get_string();
	int altitude = e.get_message()->get_map()["altitude"]->get_int();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	bool success = false;

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	if (strcmp(runway.c_str(), "") != 0)
		MessageHandler::AddRunwayToRoute(runway, fp, false);

	if (strcmp(directTo.c_str(), "") != 0)
	{
		MessageHandler::DirectTo(directTo, fp, false);
	}

	fp.GetFlightPlanData().AmendFlightPlan();
}

void MessageHandler::UpdateTrackingStatus(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	bool assumed = e.get_message()->get_map()["assumed"]->get_bool();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	bool isAssigned;

	if (assumed)
		isAssigned = fp.StartTracking();
	else
		isAssigned = fp.EndTracking();

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

//void MessageHandler::UpdatePDC(sio::event& e)
//{
//	// Get aircraft data from EXCDS
//	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
//
//	// Init Response
//	message::ptr response = object_message::create();
//	response->get_map()["callsign"] = string_message::create(callsign);
//
//	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
//
//	// Is the flight plan valid?
//	if (!FlightPlanChecks(fp, response, e)) {
//		return;
//	}
//
//	bool isAssigned;
//
//	if (!isAssigned) {
//		e.put_ack_message(NotModified(response, "Unknown reason."));
//
//		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
//		return;
//	}
//
//	// Tell EXCDS the change is done
//	response->get_map()["modified"] = bool_message::create(true);
//	e.put_ack_message(response);
//}

void MessageHandler::UpdateDirectTo(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	int altitude = e.get_message()->get_map()["altitude"]->get_int();
	std::string newDestination = e.get_message()->get_map()["new_destination"]->get_string();
	std::string route = e.get_message()->get_map()["route"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	bool isAssigned = true;

	if (strcmp(newDestination.c_str(), "") != 0)
	{
		fp.GetFlightPlanData().SetDestination(newDestination.c_str());

		MessageHandler::DirectTo(route, fp, true);
	}
	else if (route.substr(0, 3) == "DCT")
	{
		MessageHandler::DirectTo(route, fp, true);
	}
	else
	{
		fp.GetFlightPlanData().SetRoute(route.c_str());
	}

	if (altitude > 0)
		fp.GetControllerAssignedData().SetClearedAltitude(altitude);

	fp.GetFlightPlanData().AmendFlightPlan();

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

/**
* ---------------------------
* Requesting methods
*
* For EXCDS to ask for information.
* ---------------------------
*/

void MessageHandler::RequestAllAircraft(sio::event& e)
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

	#if _DEBUG
	bridgeInstance->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Msg Recieve", "All flight data requested.", true, true, true, true, true);
	#endif

	while (flightPlan.IsValid()) {
		if (flightPlan.GetState() != EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED)
		{
			#if _DEBUG
			bridgeInstance->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Data sent", std::string(std::string(flightPlan.GetCallsign()) + " was sent.").c_str(), true, true, true, true, true);
			#endif

			sio::message::ptr response = sio::object_message::create();
			response->get_map()["callsign"] = sio::string_message::create(flightPlan.GetCallsign());
			MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response);

			bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
		}
		
		flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
	}

	e.put_ack_message(bool_message::create(true));
}

void MessageHandler::RequestAircraftByCallsign(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	#if _DEBUG
	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Data sent", std::string(std::string(fp.GetCallsign()) + " was sent.").c_str(), true, true, true, true, true);
	#endif

	if (!fp.IsValid())
	{
		e.put_ack_message(NotModified(response, "Aircraft not found."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot find aircraft.", "AC_NT_FND");
		return;
	}

	MessageHandler::PrepareFlightPlanDataResponse(fp, response);
	CEXCDSBridge::GetSocket()->emit("SEND_FP_DATA", response);
}

void MessageHandler::PrepareTargetResponse(EuroScopePlugIn::CRadarTarget rt, message::ptr response)
{
	try
	{
		if (!rt.IsValid()) return;

		#if _DEBUG
		char buf[100];
		struct tm newTime;
		time_t t = time(0);

		localtime_s(&newTime, &t);
		std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
		response->get_map()["timestamp"] = string_message::create(buf);
		#endif

		// Radar & Position
		// PPS Enum
		// 0 - Not on Radar
		// 0 - SSR Correlated
		// 1 - SSR & PSR Correleated
		// 2 - SSR Uncorrelated
		// 2 - SSR & PSR Uncorrelated
		// 3 - Conflict / MSAW
		// 4 - Emergency
		// 5 - SSR Block
		// 6 - PSR Correlated VFR
		// 6 - Coasting
		// 6 - VFR
		// 7 - RVSM
		// 8 - PSR
		// 9 - Extrapolated
		// 10 - ADSB W/O RVSM
		// 11 - ADSB RVSM
		// 12 - ADSB VFR
		// 13 - ADSB Emergency
		int pps = -1;
		std::string ssr = "";
		double latitude = 0;
		double longitude = 0;

		int modec = 0;
		int vs = 0;
		int groundSpeed = 0;
		bool ADSB = false;

		if (rt.GetPosition().IsValid())
		{
			ssr = rt.GetPosition().GetSquawk();

			latitude = rt.GetPosition().GetPosition().m_Latitude;
			longitude = rt.GetPosition().GetPosition().m_Longitude;

			if (rt.GetPosition().GetFlightLevel() >= 18000)
				modec = (rt.GetPosition().GetFlightLevel() + 50) / 100;
			else
				modec = (rt.GetPosition().GetPressureAltitude() + 50) / 100;

			vs = rt.GetVerticalSpeed();

			if (rt.GetCorrelatedFlightPlan().IsValid())
			{
				switch (rt.GetPosition().GetRadarFlags())
				{
					case 
				}
			}

			//switch (rt.GetPosition().GetRadarFlags()) {
			//case 1:
			//	radarEnum = 2;
			//	break;
			//case 2:
			//	radarEnum = 3;
			//	break;
			//case 4:
			//case 7:
			//	if (rt.GetCorrelatedFlightPlan().IsValid())
			//	{
			//		EuroScopePlugIn::CFlightPlan fp = rt.GetCorrelatedFlightPlan();
			//		std::string remarks = fp.GetFlightPlanData().GetRemarks();

			//		if (remarks.find("STS/ADSB") || fp.GetFlightPlanData().GetAircraftWtc() != 'L')
			//			ADSB = true;

			//		radarEnum = 5;
			//	}
			//	else
			//	{
			//		radarEnum = 4;
			//	}
			//}
		}

		bool MEDEVAC = false;
		std::string callsign = "";
		std::string wt = "";
		std::string reportedAltitude = "";
		std::string clearedAltitude = "";
		bool altitudeError = false;
		int finalAltitude = 0;
		bool RVSM = false;
		std::string hocjs = "";
		int assignedSpeed = -1;
		int estimatedIas = 0;
		int estimatedMach = 0;

		std::string acType = "";
		std::string destination = "";
		int eta = 0;
		int distanceToDestination = 0;

		bool RNAV = false;
		std::string remarks = "";

		bool text = false;
		bool VFR = false;

		if (rt.GetCorrelatedFlightPlan().IsValid())
		{
			EuroScopePlugIn::CFlightPlan fp = rt.GetCorrelatedFlightPlan();
			std::string remarks = fp.GetFlightPlanData().GetRemarks();

			if (remarks.find("STS/MEDEVAC"))
				MEDEVAC = true;
			if (remarks.find("STS/ADSB") || fp.GetFlightPlanData().GetAircraftWtc() != 'L')
				ADSB = true;

			callsign = fp.GetCallsign();

			switch (fp.GetFlightPlanData().GetAircraftWtc())
			{
			case '?':
				wt = "?";
				break;
			case 'L':
				wt = "-";
				break;
			case 'H':
				wt = "+";
				break;
			case 'J':
				wt = "$";
				break;
			}

			reportedAltitude = fp.GetControllerAssignedData().GetFlightStripAnnotation(8);

			int tempAlt = fp.GetControllerAssignedData().GetClearedAltitude();
			if (tempAlt == 1)
				clearedAltitude = "CAPR";
			else if (tempAlt >= 0)
				clearedAltitude = "C" + std::to_string(tempAlt / 100);
			else
				clearedAltitude = "Cclr";

			if (tempAlt + 200 > modec || tempAlt - 200 < modec) {
				if (rt.GetVerticalSpeed() < 200 && rt.GetVerticalSpeed() > -200)
					altitudeError = true;
				else if (tempAlt + 200 > modec && rt.GetVerticalSpeed() > 200)
					altitudeError = true;
				else if (tempAlt - 200 < modec && rt.GetVerticalSpeed() < -200)
					altitudeError = true;
			}

			finalAltitude = fp.GetFlightPlanData().GetFinalAltitude() / 100;

			if (fp.GetCoordinatedNextControllerState() == 2 || fp.GetCoordinatedNextControllerState() == 3)
				hocjs = fp.GetHandoffTargetControllerId();

			if (fp.GetControllerAssignedData().GetAssignedMach() > 0)
				assignedSpeed = fp.GetControllerAssignedData().GetAssignedMach();
			else if (fp.GetControllerAssignedData().GetAssignedSpeed() > 0)
				assignedSpeed = fp.GetControllerAssignedData().GetAssignedSpeed();

			if (rt.GetPosition().IsValid())
			{
				estimatedIas = fp.GetFlightPlanData().PerformanceGetIas(rt.GetPosition().GetPressureAltitude(), 0);
				estimatedMach = fp.GetFlightPlanData().PerformanceGetMach(rt.GetPosition().GetFlightLevel(), 0);
			}

			acType = fp.GetFlightPlanData().GetAircraftFPType();
			destination = fp.GetFlightPlanData().GetDestination();

			eta = fp.GetPositionPredictions().GetPointsNumber();
			distanceToDestination = fp.GetDistanceToDestination();

			char capabilites = fp.GetFlightPlanData().GetCapibilities();
			switch (capabilites) {
			case 'W':
			case 'Q':
				RVSM = true;
			case 'E':
			case 'F':
			case 'R':
				ADSB = true;
			case 'Y':
			case 'C':
			case 'I':
			case 'G':
				RNAV = true;
				break;
			}
			remarks = fp.GetControllerAssignedData().GetScratchPadString();

			if (strcmp(fp.GetFlightPlanData().GetPlanType(), "V") == 0)
				VFR = true;
			if (fp.GetFlightPlanData().GetCommunicationType() == 'T')
				text = true;
		}
		//int eta = 0;
		//int distanceToDestination = 0;
		//std::string remarks = "";

		response->get_map()["radar"] =										object_message::create();
		response->get_map()["radar"]->get_map()["code"] =					int_message::create(0);
		response->get_map()["radar"]->get_map()["ssr"] =					string_message::create(ssr);
		response->get_map()["radar"]->get_map()["altitude"] =				int_message::create(modec);
		response->get_map()["radar"]->get_map()["vertical_speed"] =			int_message::create(vs);
		response->get_map()["radar"]->get_map()["ground_speed"] =			int_message::create(rt.GetGS());
		response->get_map()["radar"]->get_map()["enum"] =					int_message::create(radarEnum);

		response->get_map()["position"] =									object_message::create();
		response->get_map()["position"]->get_map()["lat"] =					double_message::create(latitude);
		response->get_map()["position"]->get_map()["long"] =				double_message::create(longitude);

		response->get_map()["mods"] =										object_message::create();
		response->get_map()["mods"]->get_map()["medevac"] =					bool_message::create(!MEDEVAC);
		response->get_map()["mods"]->get_map()["rvsm"] =					bool_message::create(RVSM);
		response->get_map()["mods"]->get_map()["adsb"] =					bool_message::create(ADSB);
		response->get_map()["mods"]->get_map()["altitude_error"] =			bool_message::create(altitudeError);
		response->get_map()["mods"]->get_map()["rnav"] =					bool_message::create(RNAV);
		response->get_map()["mods"]->get_map()["text"] =					bool_message::create(text);
		response->get_map()["mods"]->get_map()["vfr"] =						bool_message::create(VFR);

		response->get_map()["general"] =									object_message::create();
		response->get_map()["general"]->get_map()["callsign"] =				string_message::create(callsign);
		response->get_map()["general"]->get_map()["wtc"] =					string_message::create(wt);
		response->get_map()["general"]->get_map()["handoff_cjs"] =			string_message::create(hocjs);
		response->get_map()["general"]->get_map()["ac_type"] =				string_message::create(acType);
		response->get_map()["general"]->get_map()["destination"] =			string_message::create(destination);
		response->get_map()["general"]->get_map()["eta"] =					int_message::create(eta);
		response->get_map()["general"]->get_map()["distance_to_dest"] =		int_message::create(distanceToDestination);

		response->get_map()["altitude"] =									object_message::create();
		response->get_map()["altitude"]->get_map()["reported_altitude"] =	string_message::create(reportedAltitude);
		response->get_map()["altitude"]->get_map()["cleared_altitude"] =	string_message::create(clearedAltitude);
		response->get_map()["altitude"]->get_map()["filed_altitude"] =		int_message::create(finalAltitude);

		response->get_map()["speed"] =										object_message::create();
		response->get_map()["speed"]->get_map()["assigned_speed"] =			int_message::create(assignedSpeed);
		response->get_map()["speed"]->get_map()["estimated_ias"] =			int_message::create(estimatedIas);
		response->get_map()["speed"]->get_map()["estimated_mach"] =			int_message::create(estimatedMach);
	}
	catch (...)
	{
		response->get_map()["reason"] = string_message::create("Error occured while getting flight plan data.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(rt.GetSystemID(), "Error occured while getting flight plan data.", "CANT_GET_DATA");
	}
}

void MessageHandler::PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
	try
	{
		if (!fp.IsValid()) return;
		if (fp.GetState() < 2) return;

		#if _DEBUG
		char buf[100];
		struct tm newTime;
		time_t t = time(0);

		localtime_s(&newTime, &t);
		std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
		response->get_map()["timestamp"] = string_message::create(buf);
		#endif

		// Altitude Information
		int coordinated = fp.GetExitCoordinationAltitude();
		int final = fp.GetControllerAssignedData().GetFinalAltitude();
		if (final == 0)
			final = fp.GetFlightPlanData().GetFinalAltitude();
		if (coordinated == 0)
			coordinated = final;

		std::string filedAltString;
		if (final == 0)
			filedAltString = "fld";
		else if (final < 18000)
			filedAltString = "A" + std::to_string(final / 100);
		else
			filedAltString = "F" + std::to_string(final / 100);

		int clearedAlt = fp.GetControllerAssignedData().GetClearedAltitude();
		std::string clearedAltString;
		if (clearedAlt == 0)
			clearedAltString = std::to_string(final / 100); // Altitude not assigned, assume it is equal to cruise
		else if (clearedAlt == 1)
			clearedAltString = "CAPR"; // Cleared for an approach
		else if (clearedAlt == 2)
			clearedAltString = "B"; // B is used to indicate an aircraft has been cleared out of (high level) controlled airspace
		else
			clearedAltString = std::to_string(clearedAlt / 100);

		response->get_map()["altitude"] =										object_message::create();
		response->get_map()["altitude"]->get_map()["cleared"] =					string_message::create(clearedAltString);
		response->get_map()["altitude"]->get_map()["coordinated"] =				int_message::create(coordinated);
		response->get_map()["altitude"]->get_map()["filed"] =					string_message::create(filedAltString);
		response->get_map()["altitude"]->get_map()["final"] =					int_message::create(final);
		// response->get_map()["altitude"]->get_map()["reported"] =				string_message::create(fp.GetControllerAssignedData().GetFlightStripAnnotation(8));

		// Assigned Data
		response->get_map()["assigned"] =										object_message::create();
		response->get_map()["assigned"]->get_map()["heading"] =					int_message::create(fp.GetControllerAssignedData().GetAssignedHeading());
		response->get_map()["assigned"]->get_map()["squawk"] =					string_message::create(fp.GetControllerAssignedData().GetSquawk());

		// Callsign
		response->get_map()["callsign"] = string_message::create(fp.GetCallsign());

		// General
		std::string acType = fp.GetFlightPlanData().GetAircraftFPType();
		char wakeTurbulenceCat = fp.GetFlightPlanData().GetAircraftWtc();
		char equip = fp.GetFlightPlanData().GetCapibilities();
		std::string typeString = "/" + acType + "/";

		if (equip == 'Q' || equip == 'W' || equip == 'L')
			typeString += "W";
		else if (equip == 'R')
			typeString += "R";
		else if (equip == 'G' || equip == 'Y' || equip == 'C' || equip == 'I')
			typeString += "G";
		else if (equip == 'E' || equip == 'F')
			typeString += "E";
		else if (equip == 'A' || equip == 'T')
			typeString += "S";
		else
			typeString += "N";

		typeString.insert(0, 1, wakeTurbulenceCat);

		std::string speed;
		if (fp.GetControllerAssignedData().GetAssignedSpeed() > 0)
			speed = "A" + std::to_string(fp.GetControllerAssignedData().GetAssignedSpeed());
		else if (fp.GetControllerAssignedData().GetAssignedMach() > 0) {
			float mach = fp.GetControllerAssignedData().GetAssignedMach() / 100;
			speed = "A" + std::to_string(mach).substr(1);
		}
		else
			speed = std::to_string(fp.GetFlightPlanData().GetTrueAirspeed());

		response->get_map()["general"] =										object_message::create();
		response->get_map()["general"]->get_map()["aircraft_type"] =			string_message::create(typeString);
		response->get_map()["general"]->get_map()["tracking_controller"] =		string_message::create(fp.GetTrackingControllerCallsign());
		response->get_map()["general"]->get_map()["flight_rules"] =				string_message::create(fp.GetFlightPlanData().GetPlanType());
		response->get_map()["general"]->get_map()["speed"] =					string_message::create(speed);

		// Route Information
		EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
		EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

		boolean eastbound = true;
		if (origin.DirectionTo(destination) > 179)
			eastbound = false;

		response->get_map()["route"] =											object_message::create();
		response->get_map()["route"]->get_map()["arrival_airport"] =			string_message::create(fp.GetFlightPlanData().GetDestination());
		response->get_map()["route"]->get_map()["arrival_runway"] =				string_message::create(fp.GetFlightPlanData().GetArrivalRwy());
		response->get_map()["route"]->get_map()["departure_airport"] =			string_message::create(fp.GetFlightPlanData().GetOrigin());
		response->get_map()["route"]->get_map()["departure_runway"] =			string_message::create(fp.GetFlightPlanData().GetDepartureRwy());
		response->get_map()["route"]->get_map()["is_eastbound"] =				bool_message::create(eastbound);
		response->get_map()["route"]->get_map()["route"] =						string_message::create(fp.GetFlightPlanData().GetRoute());
		response->get_map()["route"]->get_map()["sid"] =						string_message::create(fp.GetFlightPlanData().GetSidName());

		// Status
		// ------------------ EXCDs Aircraft Status ------------------
		// NSTS = No status, not updated
		// ABNC = Airborne no IFR
		// CLRD = Tower has issued an IFR clearance
		// PUSH = Aircraft has been authorized to pushback (if required)
		// TXOC = Tower has taxied the aircraft
		// TXRQ = Taxiing aircraft awaiting IFR release / clearance
		// TXRL = Taxiing aircraft with valid ifr release
		// DEPA = Airborne Departure
		// ENR = Enroute
		// ARR = Within 30nm of arrival airport
		// TXIN = Taxiing in
		// PARK = Flight plan is closed

		std::string excdsGroundStatus;

		const char * groundStatus = fp.GetGroundState();
		bool cleared = fp.GetClearenceFlag();
		if (strcmp(groundStatus, "PUSH") == 0 || strcmp(groundStatus, "ARR") == 0 || strcmp(groundStatus, "TXIN") == 0 || strcmp(groundStatus, "PARK") == 0)
		{
			excdsGroundStatus = groundStatus;
		}
		else if (strcmp(groundStatus, "TAXI") == 0)
		{
			if (strcmp(fp.GetControllerAssignedData().GetScratchPadString(), "RREL") == 0)
				excdsGroundStatus = "TXRL";
			else if (cleared && strcmp(fp.GetControllerAssignedData().GetScratchPadString(), "RREQ") != 0)
				excdsGroundStatus = "TXOC";
			else
				excdsGroundStatus = "TXRQ";
		}
		else
		{
			if (cleared == false)
			{
				if (fp.GetCorrelatedRadarTarget().IsValid())
				{
					if (strcmp(groundStatus, "DEPA") == 0 && fp.GetCorrelatedRadarTarget().GetGS() >= 40)
						excdsGroundStatus = "ABNC";
				}
				else
					excdsGroundStatus = "NSTS";
			}
			else if (fp.GetDistanceFromOrigin() > 40 && fp.GetDistanceToDestination() > 40)
			{
				excdsGroundStatus = "ENR";
			}
			else
			{
				if (strcmp(groundStatus, "DEPA") == 0)
					excdsGroundStatus = "DEPA";
				else
					excdsGroundStatus = "CLRD";
			}
		}

		// Special Status (in order of presidence)
		// T = Text
		// E = Emergency (Squawking 7700)
		// C = Comm failure (Squawking 7600)
		// M = Medevac
		// R = Recieve only
		// ? = Unknown voice capability

		std::string excdsStatus;
		std::string remarks = fp.GetFlightPlanData().GetRemarks();
		const char* squawk = fp.GetCorrelatedRadarTarget().GetPosition().GetSquawk();
		char commType = fp.GetFlightPlanData().GetCommunicationType();

		if (commType == 'T')
			excdsStatus = "T";
		else if (strcmp(squawk, "7700") == 0)
			excdsStatus = "E";
		else if (strcmp(squawk, "7600") == 0)
			excdsStatus = "C";
		else if (remarks.find("STS/MEDEVAC") != std::string::npos)
			excdsStatus = "M";
		else if (commType == 'R')
			excdsStatus = "R";
		else if (commType == '?')
			excdsStatus = '?';
		else
			excdsStatus = "";

		response->get_map()["status"] =											object_message::create();
		response->get_map()["status"]->get_map()["euroscope_status"] =			int_message::create(fp.GetState());
		response->get_map()["status"]->get_map()["excds_ground_status"] =		string_message::create(excdsGroundStatus);
		response->get_map()["status"]->get_map()["excds_special_status"] =		string_message::create(excdsStatus);

		// Times
		std::string dof;
		int dofIndex = remarks.find("DOF/");
		if (dofIndex != -1)
			dof = remarks.substr(dofIndex + 4, dofIndex + 10);
		else
			dof = "";

		response->get_map()["times"] =											object_message::create();
		response->get_map()["times"]->get_map()["actual_departure_time"] =		string_message::create(fp.GetFlightPlanData().GetActualDepartureTime());
		response->get_map()["times"]->get_map()["dof"] =						string_message::create(dof);
		response->get_map()["times"]->get_map()["enroute_hours"] =				string_message::create(fp.GetFlightPlanData().GetEnrouteHours());
		response->get_map()["times"]->get_map()["enroute_minutes"] =			string_message::create(fp.GetFlightPlanData().GetEnrouteMinutes());
		response->get_map()["times"]->get_map()["filed_departure_time"] =		string_message::create(fp.GetFlightPlanData().GetEstimatedDepartureTime());

		// EXCDs estimate
		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

		std::string arrivalEstimateName = "";
		int arrivalEstimateTime = -1;

		std::string bedpost = fp.GetFlightPlanData().GetStarName();

		if (bedpost.length() > 0) {
			bedpost = bedpost.substr(0, bedpost.size() - 1);

			for (int i = 0; i < fp.GetExtractedRoute().GetPointsNumber(); i++)
			{
				if (strcmp(fp.GetExtractedRoute().GetPointName(i), bedpost.c_str()) == 0)
				{
					arrivalEstimateTime = fp.GetExtractedRoute().GetPointDistanceInMinutes(i);
					arrivalEstimateName = fp.GetExtractedRoute().GetPointName(i);
					break;
				}
			}
		}
		else if (fp.GetExtractedRoute().GetPointsNumber() > 1)
		{
			arrivalEstimateTime = fp.GetExtractedRoute().GetPointDistanceInMinutes(fp.GetExtractedRoute().GetPointsNumber() - 1);
			arrivalEstimateName = fp.GetExtractedRoute().GetPointName(fp.GetExtractedRoute().GetPointsNumber() - 1);
		}
		else
		{
			arrivalEstimateName = fp.GetFlightPlanData().GetDestination();
			arrivalEstimateTime = fp.GetPositionPredictions().GetPointsNumber() - 1;
		}

		EuroScopePlugIn::CSectorElement NDBSectorElement = bridgeInstance->SectorFileElementSelectFirst(2);

		typedef std::vector< std::tuple<std::string, int> > estimatesVectorType;
		estimatesVectorType estimateVector;

		// Determines where to place the strip
		// 0 - Place the strip in the proposed bay
		// 1 - Move the strip from proposed to its active panel (if available)
		// 2 - Place the strip in the inbox
		// 3 - Do not place the strip in any inbox
		// Otherwise place it in the bay that it will pass abeam to in the shortest amount of time
		std::string bay = "0";
		int bayEstimateTime = 500;

		if (fp.GetSectorEntryMinutes() > 0)
			bay = "2";
		
		response->get_map()["excds_estimates"] = object_message::create();
		response->get_map()["excds_estimates"]->get_map()["estimates"] = object_message::create();

		if (strcmp(fp.GetGroundState(), "PARK") == 0 || (fp.GetDistanceToDestination() < 3 && fp.GetDistanceFromOrigin() > 3))
			bay = "3";

		while (NDBSectorElement.IsValid())
		{
			EuroScopePlugIn::CPosition NDBPosition;
			NDBSectorElement.GetPosition(&NDBPosition, 0);
			double previousDistanceToNDB = fp.GetPositionPredictions().GetPosition(0).DistanceTo(NDBPosition);

			for (int i = 1; i < fp.GetPositionPredictions().GetPointsNumber(); i++)
			{
				if (previousDistanceToNDB > fp.GetPositionPredictions().GetPosition(i).DistanceTo(NDBPosition))
				{
					previousDistanceToNDB = fp.GetPositionPredictions().GetPosition(i).DistanceTo(NDBPosition);
				}
				else if (i == 1 || fp.GetPositionPredictions().GetPosition(i).DistanceTo(NDBPosition) > 200)
				{
					break;
				}
				else
				{
					response->get_map()["excds_estimates"]->get_map()["estimates"]->get_map()[NDBSectorElement.GetName()] = int_message::create(i);

					if (i < bayEstimateTime)
					{
						bayEstimateTime = i;
						bay = NDBSectorElement.GetName();
					}

					break;
				}
			}

			NDBSectorElement = bridgeInstance->SectorFileElementSelectNext(NDBSectorElement, 1);
		}

		if (strcmp(fp.GetGroundState(), "TAXI") == 0)
		{
			bay = "1";
		}

		response->get_map()["excds_estimates"]->get_map()["arrival_time"] =		int_message::create(arrivalEstimateTime);
		response->get_map()["excds_estimates"]->get_map()["arrival_fix"] =		string_message::create(arrivalEstimateName);
		response->get_map()["excds_estimates"]->get_map()["bay"] =				string_message::create(bay);
		response->get_map()["excds_estimates"]->get_map()["groundstatus"] =		string_message::create(fp.GetGroundState());

		response->get_map()["success"] = bool_message::create(true);
	}
	catch (...)
	{
		response->get_map()["reason"] = string_message::create("Error occured while getting flight plan data.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error occured while getting flight plan data.", "CANT_GET_DATA");
	}
}

void MessageHandler::RequestDirectTo(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

#if _DEBUG
	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Data sent", std::string(std::string(fp.GetCallsign()) + " was sent.").c_str(), true, true, true, true, true);
#endif

	if (!fp.IsValid())
	{
		e.put_ack_message(NotModified(response, "Aircraft not found."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot find aircraft.", "AC_NT_FND");
		return;
	}

	try {
		// General
		std::string equipment = fp.GetFlightPlanData().GetAircraftInfo();
		int equipmentIndex = equipment.find_last_of('/') + 1;
		if (equipmentIndex < 1)
			equipment = "N";
		else
			equipment = equipment.substr(equipmentIndex);

		response->get_map()["general"] = object_message::create();
		response->get_map()["general"]->get_map()["equipment"] = string_message::create(equipment);
		response->get_map()["general"]->get_map()["speed"] = int_message::create(fp.GetFlightPlanData().GetTrueAirspeed());
		response->get_map()["general"]->get_map()["transponder"] = string_message::create(fp.GetControllerAssignedData().GetSquawk());
		response->get_map()["general"]->get_map()["type"] = string_message::create(fp.GetFlightPlanData().GetAircraftFPType());

		// Route
		response->get_map()["route"] = object_message::create();
		response->get_map()["route"]->get_map()["destination"] = string_message::create(fp.GetFlightPlanData().GetDestination());
		response->get_map()["route"]->get_map()["origin"] = string_message::create(fp.GetFlightPlanData().GetOrigin());
		response->get_map()["route"]->get_map()["route"] = string_message::create(fp.GetFlightPlanData().GetRoute());

		// Altitude information
		int final = fp.GetControllerAssignedData().GetFinalAltitude();
		std::string filedAltString;
		if (final == 0)
			final = fp.GetFlightPlanData().GetFinalAltitude();

		if (final == 0)
			filedAltString = "fld";
		else if (final < 18000)
			filedAltString = "A" + std::to_string(final / 100);
		else
			filedAltString = "F" + std::to_string(final / 100);

		int clearedAlt = fp.GetControllerAssignedData().GetClearedAltitude();
		std::string clearedAltString;
		if (clearedAlt == 0)
			clearedAltString = std::to_string(final / 100); // Altitude not assigned, assume it is equal to cruise
		else if (clearedAlt == 1)
			clearedAltString = "CAPR"; // Cleared for an approach
		else if (clearedAlt == 2)
			clearedAltString = "B"; // B is used to indicate an aircraft has been cleared out of (high level) controlled airspace
		else
			clearedAltString = std::to_string(clearedAlt / 100);

		response->get_map()["altitude"] = object_message::create();
		response->get_map()["altitude"]->get_map()["filed"] = string_message::create(filedAltString.c_str());
		response->get_map()["altitude"]->get_map()["cleared"] = string_message::create(filedAltString.c_str());

		response->get_map()["success"] = bool_message::create(true);

		e.put_ack_message(response);
	}
	catch (...)
	{
		response->get_map()["reason"] = string_message::create("Error occured while getting flight plan data.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error occured while getting flight plan data.", "CANT_GET_DATA");

		e.put_ack_message(response);
	}
}

/**
* ---------------------------
* Internal methods
*
* Methods only used for this class!
* ---------------------------
*/

/*
* This is used to check if a callsign has a flight plan to modify, and we (the controller) are able to modify it.
* Returns true if the flight plan is valid.
*/
bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, message::ptr response, sio::event& e)
{
	#if _DEBUG
	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Msg Recieve", e.get_message()->get_map()["message"]->get_string().c_str(), true, true, true, true, true);
	#endif

	// Does the flight plan exist?
	if (!fp.IsValid())
	{
		e.put_ack_message(NotModified(response, "Flight plan not found."));

		CEXCDSBridge::SendEuroscopeMessage(response->get_map()["callsign"]->get_string().c_str(), "Cannot modify.", "NO_FPLN");
		return false;
	}

	// Are we allowed to modify this aircraft?
	if (!fp.GetTrackingControllerIsMe() && strlen(fp.GetTrackingControllerId()) != 0)
	{
		e.put_ack_message(NotModified(response, "Aircraft is being tracked by another controller."));

		CEXCDSBridge::SendEuroscopeMessage(response->get_map()["callsign"]->get_string().c_str(), "Cannot modify.", "ALRDY_TRAKD");
		return false;
	}
	
	return true;
}

/*
* Tell the websocket the aircraft was not modified.
*/
message::ptr MessageHandler::NotModified(message::ptr response, std::string reason)
{
	response->get_map()["modified"] = bool_message::create(false);
	response->get_map()["reason"] = string_message::create(reason);

	return response;
}

/*
* Adds the specified runway to the incoming route, depending on if it is departing or arriving.
* 
* This is so EuroScope can interpret the runway change properly.
*/
std::string MessageHandler::AddRunwayToRoute(std::string runway, EuroScopePlugIn::CFlightPlan fp, bool departure)
{
	std::string route = fp.GetFlightPlanData().GetRoute();
	#if _DEBUG
	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", std::string("ROUTE (" + std::string(fp.GetCallsign()) + ")").c_str(), route.c_str(), true, true, true, true, true);
	#endif

	// Store the SID or STAR depending on what we are looking for.
	std::string procedure = departure ? fp.GetFlightPlanData().GetSidName() : fp.GetFlightPlanData().GetStarName();
	std::string airport = departure ? fp.GetFlightPlanData().GetOrigin() : fp.GetFlightPlanData().GetDestination();

	/*
	* Euroscope uses the format "{PROCEDURE}/{RUNWAY}" when determining runway assignments. If there is no procedure,
	* the format "{AIRPORT}/{RUNWAY}" is used.
	*/
	std::string runwayAssignment;
	if (procedure != "")
	{
		// There should always be a procedure. `.GetStarName()` and `.GetSidName()` always return one if it is in the flight plan,
		// and registered in the sector file.
		
		// Regex replaces the current procedure name, and any runway with it.
		// If the procedure is not found, it will proceed to the bottom of this method.
		runwayAssignment = procedure + "/" + runway;
		if (route.find(procedure) != std::string::npos)
		{
			route = regex_replace(route, std::regex("(" + procedure + "|" + airport + ")" + "/?[0-3]?[0-9]?"), runwayAssignment);

			#if _DEBUG
			CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", std::string("MOD ROUTE (" + std::string(fp.GetCallsign()) + ")").c_str(), route.c_str(), true, true, true, true, true);
			#endif
			
			return route;
		}
	}
	else
	{
		runwayAssignment = airport + "/" + runway;

		// Is the airport already in the route string?
		if (route.find(airport) != std::string::npos)
		{
			// Regex will replace the current airport, and any runway already with it.
			route = regex_replace(route, std::regex("(" + airport + ")" + "/?[0-3]?[0-9]?"), runwayAssignment);
			
			#if _DEBUG
			CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", std::string("MOD ROUTE (" + std::string(fp.GetCallsign()) + ")").c_str(), route.c_str(), true, true, true, true, true);
			#endif

			return route;
		}
	}

	// Add the new runway assignment to the route as it isn't there already
	if (departure)
	{
		route = runwayAssignment + " " + route;
	}
	else
	{
		route += " " + runwayAssignment;
	}

	#if _DEBUG
	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", std::string("MOD ROUTE (" + std::string(fp.GetCallsign()) + ")").c_str(), route.c_str(), true, true, true, true, true);
	#endif

	return route;
}

void MessageHandler::DirectTo(std::string waypoint, EuroScopePlugIn::CFlightPlan fp, bool newRoute = false)
{
	if (!fp.IsValid()) return;

	fp.GetControllerAssignedData().SetDirectToPointName(waypoint.c_str());

	std::string presentPosition;
	float lat, lon, latmin, lonmin;
	double longitudedecmin = 0;
	double latitudedecmin = 0;
	bool hasPosition = false;

	if (fp.GetCorrelatedRadarTarget().IsValid())
	{
		double longitudedecmin = modf(fp.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude, &lon);
		double latitudedecmin = modf(fp.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude, &lat);
		hasPosition = true;
	}
	else if (fp.GetFPTrackPosition().IsValid())
	{
		double longitudedecmin = modf(fp.GetFPTrackPosition().GetPosition().m_Longitude, &lon);
		double latitudedecmin = modf(fp.GetFPTrackPosition().GetPosition().m_Latitude, &lat);
		hasPosition = true;
	}

	if (hasPosition)
	{
		latmin = abs(round(latitudedecmin * 60));
		lonmin = abs(round(longitudedecmin * 60));
		std::string lonString = std::to_string(static_cast<int>(abs(lon)));
		if (lonString.size() < 3)
		{
			lonString.insert(lonString.begin(), 3 - lonString.size(), '0');
		}

		std::string latSuffix = (lat > 0) ? "N" : "S";
		std::string lonSuffix = (lon > 0) ? "E" : "W";
		presentPosition = std::to_string(static_cast<int>(lat)) + std::to_string(static_cast<int>(latmin)) + latSuffix + lonString + std::to_string(static_cast<int>(lonmin)) + lonSuffix + " ";
		
		if (!newRoute)
		{
			std::string routeString = fp.GetFlightPlanData().GetRoute();
			auto itr = routeString.find(waypoint.c_str());
			if (itr != routeString.npos)
			{
				routeString = routeString.substr(itr);
				routeString.insert(0, presentPosition);
			}
			else
			{
				routeString = presentPosition;
				for (int i = fp.GetExtractedRoute().GetPointsAssignedIndex(); i < fp.GetExtractedRoute().GetPointsNumber(); i++) {
					std::string waypoint = fp.GetExtractedRoute().GetPointName(i);
					routeString += waypoint + " ";
				}
			}

			fp.GetFlightPlanData().SetRoute(routeString.c_str());
		}
		else
		{
			std::string newRouteString = presentPosition + waypoint.substr(4);
			fp.GetFlightPlanData().SetRoute(newRouteString.c_str());
		}
		fp.GetFlightPlanData().AmendFlightPlan();
	}
}

bool MessageHandler::StatusAssign(std::string status, EuroScopePlugIn::CFlightPlan fp, std::string departureTime = "")
{
	// ------------------ EXCDs Aircraft Status ----------------
	// NSTS = No status, not updated
	// ABNC = Airborne no IFR
	// CLRD = Tower has issued an IFR clearance
	// PUSH = Aircraft has been authorized to pushback (if required)
	// TXOC = Tower has taxied the aircraft
	// TXRQ = Taxiing aircraft awaiting IFR release // clearance
	// TXRL = Taxiing aircraft with valid ifr release
	// DEPA = Airborne
	// ARR = Within 30nm of arrival airport
	// TXIN = Taxiing in
	// PARK = Flight plan is closed

	bool success = false;

	if (strcmp(status.c_str(), "NSTS") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("NSTS");
		success = fp.GetControllerAssignedData().SetScratchPadString("NOTC");
	}
	else if (strcmp(status.c_str(), "CLRD") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("CLEA");
	}
	else if (strcmp(status.c_str(), "PUSH") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("PUSH");
	}
	else if (strcmp(status.c_str(), "TXOC") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("CLEA");
		success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
	}
	else if (strcmp(status.c_str(), "TXRQ") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
		success = fp.GetControllerAssignedData().SetScratchPadString("RREQ");
	}
	else if (strcmp(status.c_str(), "TXRL") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
		success = fp.GetControllerAssignedData().SetScratchPadString("CLEA");

		OutputDebugString("TXRL");

		// 1 = FSS / 5 = APP/DEP / 6 = CTR
		if (CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() > 1 && CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() < 5)
			return false;

		success = fp.GetControllerAssignedData().SetScratchPadString("RREL");
	}
	else if (strcmp(status.c_str(), "DEPA") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("DEPA");
		success = fp.GetControllerAssignedData().SetScratchPadString("CLEA");

		std::string recDepartureTime = fp.GetFlightPlanData().GetActualDepartureTime();

		if (departureTime.length() == 4)
			success = fp.GetFlightPlanData().SetActualDepartureTime(departureTime.c_str());
		else if (recDepartureTime.length() == 4)
			success = fp.GetFlightPlanData().SetActualDepartureTime(recDepartureTime.c_str());
		else
			fp.GetFlightPlanData().SetActualDepartureTime(fp.GetFlightPlanData().GetActualDepartureTime());

		fp.GetControllerAssignedData().SetScratchPadString("");
	}
	else if (strcmp(status.c_str(), "ARR") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("ARR");
	}
	else if (strcmp(status.c_str(), "TXIN") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("TXIN");
	}
	else if (strcmp(status.c_str(), "PARK") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("PARK");
	}
	else
	{
		success = false;
	}

	return success;
}

void MessageHandler::ForceFlightPlanRefresh(EuroScopePlugIn::CFlightPlan fp)
{
	fp.GetFlightPlanData().SetOrigin(fp.GetFlightPlanData().GetOrigin());
}

void MessageHandler::MissedApproach(EuroScopePlugIn::CFlightPlan fp)
{
	/* --- MISSED APPROACH FUNCTION ---
	* CAATS removes any STAR from the route, inserts the destination airport into the route, followed by a point 10nm north of the airport at 5000'
	*/

	if (!fp.IsValid()) return;

	std::string route = fp.GetFlightPlanData().GetRoute();

	if (strcmp(fp.GetFlightPlanData().GetStarName(), "") != 0)
	{
		route.erase(route.find_last_of(" "));
	}

	EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

	std::string destinationAirportCoordinates;
	float lat, lon, latmin, lonmin;
	double longitudedecmin = 0;
	double latitudedecmin = 0;
}

std::string MessageHandler::SquawkGenerator(std::string squawkPrefix)
{
	std::string transponder;

	for (int i = 1; i <= 77; i++)
	{
		std::string squawkSuffix = std::to_string(i);
		squawkSuffix.insert(squawkSuffix.begin(), 2 - squawkSuffix.length(), '0'); // Add a leading 0 if it's 1 digit
		transponder = squawkPrefix + squawkSuffix;

		bool valid = true;
		for (
			EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelectFirst();
			fp.IsValid();
			fp = CEXCDSBridge::GetInstance()->FlightPlanSelectNext(fp)
		) {

			if (strcmp(fp.GetControllerAssignedData().GetSquawk(), transponder.c_str()) == 0) {
				valid = false;
				break;
			}
		}

		if (valid) {
			return transponder;
		}
	}

	return "1001";
}