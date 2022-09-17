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
	int expected = e.get_message()->get_map()["expected"]->get_int();
	int coordinated = e.get_message()->get_map()["coordinated"]->get_int();
	// Warning, wrong way, vfr etc.
	std::string warning = e.get_message()->get_map()["warning"]->get_string();
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
	if (final != -1)
		fp.GetFlightPlanData().SetFinalAltitude(final);
	if (expected != -1)
		fp.GetControllerAssignedData().SetFinalAltitude(expected);
	if (coordinated != -1)
		fp.InitiateCoordination(fp.GetCoordinatedNextController(), fp.GetNextCopxPointName(), coordinated);
	if (warning != "") {
		if (warning == "CLR")
			fp.GetControllerAssignedData().SetFlightStripAnnotation(7, "");
		else
			fp.GetControllerAssignedData().SetFlightStripAnnotation(7, warning.c_str());
	}
	if (reported != "")
		fp.GetControllerAssignedData().SetFlightStripAnnotation(8, reported.c_str());

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
	bool isAssigned;
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
	std::string id = e.get_message()->get_map()["id"]->get_string();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	int assignedMach = e.get_message()->get_map()["assignedMach"]->get_int();
	int assignedSpeed = e.get_message()->get_map()["assignedSpeed"]->get_int();
	int filedSpeed = e.get_message()->get_map()["filedSpeed"]->get_int();

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

void MessageHandler::UpdateStripAnnotation(sio::event& e)
{
	// Parse data from EXCDS
	int index = e.get_message()->get_map()["index"]->get_int();
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string annotation = e.get_message()->get_map()["value"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);
	
	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	if (fp.GetControllerAssignedData().SetFlightStripAnnotation(index, annotation.c_str())) {
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	}
}

void MessageHandler::UpdateSquawk(sio::event& e)
{
	// Parse data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string squawkPrefix = e.get_message()->get_map()["prefix"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	if (fp.GetControllerAssignedData().SetSquawk(MessageHandler::SquawkGenerator(squawkPrefix).c_str())) {
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	}
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

	bool success;

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

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
	else if (strcmp(status.c_str(), "TXOA") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("NOTC");
		success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
	}
	else if (strcmp(status.c_str(), "DEPA") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("DEPA");
		success = fp.GetFlightPlanData().SetActualDepartureTime(departureTime.c_str());
	}
	else if (strcmp(status.c_str(), "ARR") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("NOTC");
		success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
	}
	else if (strcmp(status.c_str(), "TXIN") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("TXIN");
	}
	else if (strcmp(status.c_str(), "CLSD") == 0)
	{
		success = fp.GetControllerAssignedData().SetScratchPadString("PARK");
	}

	if (success) {
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	else {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
	}
}

void MessageHandler::PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
	try
	{
		if (!fp.IsValid()) return;
		#if _DEBUG
		char buf[100];
		struct tm newTime;
		time_t t = time(0);

		localtime_s(&newTime, &t);
		std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
		response->get_map()["timestamp"] = string_message::create(buf);
		#endif

		std::string assignedSquawk(fp.GetControllerAssignedData().GetSquawk());
		if (assignedSquawk == "" || assignedSquawk.substr(2) == "00") {
			fp.GetControllerAssignedData().SetSquawk(MessageHandler::SquawkGenerator("20").c_str());
		}
		
		// Flight Plan Data
		response->get_map()["flight_plan"] =									object_message::create();
		response->get_map()["flight_plan"]->get_map()["aircraft_type"] =		string_message::create(fp.GetFlightPlanData().GetAircraftFPType());
		response->get_map()["flight_plan"]->get_map()["alternate"] =			string_message::create(fp.GetFlightPlanData().GetAlternate());
		response->get_map()["flight_plan"]->get_map()["arrival_airport"] =		string_message::create(fp.GetFlightPlanData().GetDestination());
		response->get_map()["flight_plan"]->get_map()["arrival_runway"] =		string_message::create(fp.GetFlightPlanData().GetArrivalRwy());
		response->get_map()["flight_plan"]->get_map()["departure_airport"] =	string_message::create(fp.GetFlightPlanData().GetOrigin());
		response->get_map()["flight_plan"]->get_map()["departure_runway"] =		string_message::create(fp.GetFlightPlanData().GetDepartureRwy());
		response->get_map()["flight_plan"]->get_map()["departure_time"] =		string_message::create(fp.GetFlightPlanData().GetEstimatedDepartureTime());
		response->get_map()["flight_plan"]->get_map()["enroute_hours"] =		string_message::create(fp.GetFlightPlanData().GetEnrouteHours());
		response->get_map()["flight_plan"]->get_map()["enroute_minutes"] =		string_message::create(fp.GetFlightPlanData().GetEnrouteMinutes());
		response->get_map()["flight_plan"]->get_map()["equipment_code"] =		string_message::create(std::string(1, fp.GetFlightPlanData().GetCapibilities()));
		response->get_map()["flight_plan"]->get_map()["filed_speed"] =			int_message::create(fp.GetFlightPlanData().GetTrueAirspeed());
		response->get_map()["flight_plan"]->get_map()["final_altitude"] =		int_message::create(fp.GetFlightPlanData().GetFinalAltitude());
		response->get_map()["flight_plan"]->get_map()["flight_rules"] =			string_message::create(fp.GetFlightPlanData().GetPlanType());
		response->get_map()["flight_plan"]->get_map()["remarks"] =				string_message::create(fp.GetFlightPlanData().GetRemarks());
		response->get_map()["flight_plan"]->get_map()["sid"] =					string_message::create(fp.GetFlightPlanData().GetSidName());
		response->get_map()["flight_plan"]->get_map()["voice_capability"] =		string_message::create(std::string(1, fp.GetFlightPlanData().GetCommunicationType()));
		response->get_map()["flight_plan"]->get_map()["wake_turbulence"] =		string_message::create(std::string(1, fp.GetFlightPlanData().GetAircraftWtc()));
		
		// Controller Assigned Data
		response->get_map()["assigned"] =										object_message::create();
		response->get_map()["assigned"]->get_map()["assigned_heading"] =		int_message::create(fp.GetControllerAssignedData().GetAssignedHeading());
		response->get_map()["assigned"]->get_map()["assigned_mach"] =			int_message::create(fp.GetControllerAssignedData().GetAssignedMach());
		response->get_map()["assigned"]->get_map()["assigned_speed"] =			int_message::create(fp.GetControllerAssignedData().GetAssignedSpeed());
		response->get_map()["assigned"]->get_map()["assigned_squawk"] =			string_message::create(fp.GetControllerAssignedData().GetSquawk());
		response->get_map()["assigned"]->get_map()["clearance_flag"] =			bool_message::create(fp.GetClearenceFlag());
		response->get_map()["assigned"]->get_map()["coordinated_altitude"] =	int_message::create(fp.GetExitCoordinationAltitude());
		response->get_map()["assigned"]->get_map()["departure_time"] =			string_message::create(fp.GetFlightPlanData().GetActualDepartureTime());
		response->get_map()["assigned"]->get_map()["final_altitude"] =			int_message::create(fp.GetControllerAssignedData().GetFinalAltitude());
		response->get_map()["assigned"]->get_map()["ground_status"] =			string_message::create(fp.GetGroundState());
		response->get_map()["assigned"]->get_map()["scratchpad"] =				string_message::create(fp.GetControllerAssignedData().GetScratchPadString());
		response->get_map()["assigned"]->get_map()["temporary_altitude"] =		int_message::create(fp.GetControllerAssignedData().GetClearedAltitude());

		// Euroscope Data
		response->get_map()["euroscope"] =										object_message::create();
		response->get_map()["euroscope"]->get_map()["aircraft_state"] =			int_message::create(fp.GetState()); // Non-concerned, transfered, etc.
		
		EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
		EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);
		response->get_map()["euroscope"]->get_map()["direction_to"] =			 double_message::create(origin.DirectionTo(destination));

		// Time to destination in minutes
		if (fp.GetCorrelatedRadarTarget().GetGS() > 40)
			response->get_map()["euroscope"]->get_map()["time_to_destination"] = int_message::create(fp.GetPositionPredictions().GetPointsNumber());
		else
			response->get_map()["euroscope"]->get_map()["time_to_destination"] = double_message::create(-1);
		
		response->get_map()["euroscope"]->get_map()["ground_speed"] =		 	 int_message::create(fp.GetCorrelatedRadarTarget().GetGS());
		response->get_map()["euroscope"]->get_map()["sector_entry_time"] =		 int_message::create(fp.GetSectorEntryMinutes()); // -1 if it will never enter, 0 if it's already in
		response->get_map()["euroscope"]->get_map()["sector_exit_time"] =		 int_message::create(fp.GetSectorExitMinutes()); // -1 if it will never enter

		// Position Predictions and Route
		message::ptr positionPredictionArray = array_message::create();
		for (int i = 0; i < fp.GetExtractedRoute().GetPointsNumber() - 1; i++)
		{
			message::ptr positionObject = object_message::create();
			positionObject->get_map()["point_name"] = string_message::create(fp.GetExtractedRoute().GetPointName(i));
			positionObject->get_map()["time"] = int_message::create(fp.GetExtractedRoute().GetPointDistanceInMinutes(i));

			positionPredictionArray->get_vector().push_back(positionObject);
		}
		response->get_map()["position_predictions"] = positionPredictionArray;

		// Flight Strip Annotations
		response->get_map()["strip_annotations"] = object_message::create();
		for (int i = 0; i <= 8; i++) {
			response->get_map()["strip_annotations"]->get_map()[std::to_string(i)] = string_message::create(fp.GetControllerAssignedData().GetFlightStripAnnotation(i));
		}

		response->get_map()["success"] = bool_message::create(true);
		
	}
	catch (...)
	{
		response->get_map()["reason"] = string_message::create("Error occured while getting flight plan data.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error occured while getting flight plan data.", "CANT_GET_DATA");
	}
}
/*
void MessageHandler::RequestSids(sio::event& e) {
	for (
		EuroScopePlugIn::CSectorElement element = CEXCDSBridge::GetInstance()->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS);
		element.IsValid();
		element = CEXCDSBridge::GetInstance()->SectorFileElementSelectNext(element, EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS)
	) {
		EuroScopePlugIn::CPosition position;
		element.GetPosition(&position, 0);

		char output[512];
		sprintf_s(output, "%s | %s | %s | %f \n",
			element.GetName(),
			element.GetAirportName(),
			element.GetRunwayName(0),
			position.m_Latitude
		);

		OutputDebugString(output);
	}
}
*/

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

void MessageHandler::DirectTo(std::string waypoint, EuroScopePlugIn::CFlightPlan fp)
{
	if (!fp.IsValid()) return;

	float lat, lon, latmin, lonmin;
	double latitude, longitude;

	if (fp.GetCorrelatedRadarTarget().IsValid()) {

	}
	else if (fp.GetFPTrackPosition().IsValid()) {

	}
	else {

	}
}

std::string MessageHandler::SquawkGenerator(std::string squawkPrefix) {
	std::string transponder;

	for (int i = 1; i < 78; i++)
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

