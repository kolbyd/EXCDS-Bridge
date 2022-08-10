#include "pch.h"
#include "sio_client.h"
#include <iostream>
#include <string>
#include <regex>
#include <time.h>

#include "EuroScopePlugIn.h"
#include "CEXCDSBridge.h"

#include "MessageHandler.h"

using namespace sio;

/**
* ---------------------------
* Updating methods
* 
* These are for EXCDS to tell us to update aircraft in EuroScope.
* ---------------------------
*/
void MessageHandler::UpdateGroundStatus(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string groundStatus = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	/*
	* Is the status valid ?
	* 
	* ref: https://www.euroscope.hu/wp/non-standard-extensions/
	* ST-UP = Startup Approved
	* PUSH  = Pushback Approved
	* TAXI  = Taxiing
	* DEPA  = Departing/Take Off
	*/
	if (!regex_match(groundStatus, std::regex("^(ST-UP|PUSH|TAXI|DEPA)$")))
	{
		e.put_ack_message(NotModified(response, "Ground status is invalid."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "INVLD_GND_STS");
		return;
	}

	// Assign the status!
	bool isAssigned = fp.GetControllerAssignedData().SetScratchPadString(groundStatus.c_str());
	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

void MessageHandler::UpdateClearance(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	bool clearanceFlag = e.get_message()->get_map()["value"]->get_bool();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	/*
	* ref: https://www.euroscope.hu/wp/non-standard-extensions/
	* 
	* CLEA = Clearance received
	* NOTC = Clearance not received
	* 
	* Oh Euroscope...
	*/
	bool isAssigned = fp.GetControllerAssignedData().SetScratchPadString(clearanceFlag ? "CLEA" : "NOTC");
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
	int altitude = e.get_message()->get_map()["value"]->get_int();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	// Check update type from it's id
	bool isAssigned;
	if (id == "UPDATE_TEMPORARY_ALTITUDE")
	{
		isAssigned = fp.GetControllerAssignedData().SetClearedAltitude(altitude);
	}
	else if (id == "UPDATE_FINAL_ALTITUDE")
	{
		isAssigned = fp.GetControllerAssignedData().SetFinalAltitude(altitude);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown update type."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN_TYPE");
		return;
	}

	// Check if it assigned properly
	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

[[deprecated("Not going to be used.")]]
__declspec(deprecated) void MessageHandler::UpdateSquawk(sio::event& e)
{
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string squawk = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);
	
	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
	
	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	// Is squawk code valid?
	if (!regex_match(squawk, std::regex("^[0-7]{4}$")))
	{
		e.put_ack_message(NotModified(response, "Squawk code is invalid."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "INVLD_SQWK");
		return;
	}

	// Everything is good, assign the new squawk!
	bool isAssigned = fp.GetControllerAssignedData().SetSquawk(squawk.c_str());
	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
}

[[deprecated("Not going to be used.")]]
__declspec(deprecated) void MessageHandler::UpdateRunway(sio::event& e)
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

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN_TYPE");
		return;
	}

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
	std::string newRunway = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	// TODO: This needs to be done.
}

/**
* ---------------------------
* Requesting methods
*
* For EXCDS to ask for information.
* ---------------------------
*/

void MessageHandler::RequestAircraftInformation(sio::event& e)
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

	#if _DEBUG
	bridgeInstance->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Msg Recieve", "All flight data requested.", true, true, true, true, true);
	#endif

	while (flightPlan.IsValid()) {
		#if _DEBUG
		bridgeInstance->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Data sent", std::string(std::string(flightPlan.GetCallsign()) + " was sent.").c_str(), true, true, true, true, true);
		#endif

		sio::message::ptr response = sio::object_message::create();
		response->get_map()["callsign"] = sio::string_message::create(flightPlan.GetCallsign());
		MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response);
		
		bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
		
		flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
	}
}

void MessageHandler::PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
	try
	{
		#if _DEBUG
		char buf[100];
		struct tm newTime;
		time_t t = time(0);

		localtime_s(&newTime, &t);
		std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
		response->get_map()["timestamp"] = string_message::create(buf);
		#endif
		
		// Flight Plan Data
		response->get_map()["flight_plan"] =									object_message::create();
		response->get_map()["flight_plan"]->get_map()["aircraft_type"] =		string_message::create(fp.GetFlightPlanData().GetAircraftFPType());
		response->get_map()["flight_plan"]->get_map()["arrival_airport"] =		string_message::create(fp.GetFlightPlanData().GetDestination());
		response->get_map()["flight_plan"]->get_map()["arrival_runway"] =		string_message::create(fp.GetFlightPlanData().GetArrivalRwy());
		response->get_map()["flight_plan"]->get_map()["departure_airport"] =	string_message::create(fp.GetFlightPlanData().GetOrigin());
		response->get_map()["flight_plan"]->get_map()["departure_runway"] =		string_message::create(fp.GetFlightPlanData().GetDepartureRwy());
		response->get_map()["flight_plan"]->get_map()["equipment_code"] =		string_message::create(std::string(1, fp.GetFlightPlanData().GetCapibilities()));
		response->get_map()["flight_plan"]->get_map()["filed_speed"] =			int_message::create(fp.GetFlightPlanData().GetTrueAirspeed());
		response->get_map()["flight_plan"]->get_map()["final_altitude"] =		int_message::create(fp.GetFlightPlanData().GetFinalAltitude());
		response->get_map()["flight_plan"]->get_map()["remarks"] =				string_message::create(fp.GetFlightPlanData().GetRemarks());
		response->get_map()["flight_plan"]->get_map()["route"] =				string_message::create(fp.GetFlightPlanData().GetRoute());
		response->get_map()["flight_plan"]->get_map()["wake_turbulence"] =		string_message::create(std::string(1, fp.GetFlightPlanData().GetAircraftWtc()));
		
		// Controller Assigned Data
		response->get_map()["assigned"] =										object_message::create();
		response->get_map()["assigned"]->get_map()["assigned_mach"] =			int_message::create(fp.GetControllerAssignedData().GetAssignedMach());
		response->get_map()["assigned"]->get_map()["assigned_speed"] =			int_message::create(fp.GetControllerAssignedData().GetAssignedSpeed());
		response->get_map()["assigned"]->get_map()["assigned_squawk"] =			string_message::create(fp.GetControllerAssignedData().GetSquawk());
		response->get_map()["assigned"]->get_map()["ground_status"] =			string_message::create(fp.GetGroundState());
		response->get_map()["assigned"]->get_map()["scratchpad"] =				string_message::create(fp.GetControllerAssignedData().GetScratchPadString());
		response->get_map()["assigned"]->get_map()["temporary_altitude"] =		int_message::create(fp.GetControllerAssignedData().GetClearedAltitude());

		// Euroscope Data
		response->get_map()["euroscope"] =										object_message::create();
		response->get_map()["euroscope"]->get_map()["aircraft_state"] =			int_message::create(fp.GetState()); // Non-concerned, transfered, etc.
		response->get_map()["euroscope"]->get_map()["ground_speed"] =			int_message::create(fp.GetCorrelatedRadarTarget().GetGS());
		response->get_map()["euroscope"]->get_map()["next_point"] =				string_message::create(fp.GetExtractedRoute().GetPointName(fp.GetExtractedRoute().GetPointsCalculatedIndex()));
		response->get_map()["euroscope"]->get_map()["next_point_estimate"] =	int_message::create(fp.GetExtractedRoute().GetPointDistanceInMinutes(fp.GetExtractedRoute().GetPointsCalculatedIndex()));
		response->get_map()["euroscope"]->get_map()["sector_entry_time"] =		int_message::create(fp.GetSectorEntryMinutes()); // -1 if it will never enter, 0 if it's already in
		response->get_map()["euroscope"]->get_map()["track"] =					double_message::create(fp.GetCorrelatedRadarTarget().GetTrackHeading());

		response->get_map()["success"] = bool_message::create(true);
		
	}
	catch (...)
	{
		response->get_map()["reason"] = string_message::create("Error occured while getting flight plan data.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error occured while getting flight plan data.", "CANT_GET_DATA");
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

