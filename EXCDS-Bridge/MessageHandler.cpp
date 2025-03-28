#include "sio_client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <time.h>
#include <vector>
#include <Windows.h>

#include "ApiHelper.h"
#include "EuroScopePlugIn.h"
#include "CEXCDSBridge.h"

#include "MessageHandler.h"

#define ENTER 0x1c

using namespace sio;

/**
* ---------------------------
* Updating methods
*
* These are for EXCDS to tell us to update aircraft in EuroScope.
* ---------------------------
*/

typedef std::tuple < std::string, EuroScopePlugIn::CPosition> EstimatePosn;
std::vector <EstimatePosn> estimates;

#pragma region Update_Methods

void MessageHandler::UpdatePositions(sio::event& e)
{
	try {
		typedef std::tuple <std::string, std::string, std::string> pos;
		// Send the positions as a JSON formatted tuple array | 0 is name, 1 is lat, 2 is lon
		std::vector<message::ptr> positions = e.get_message()->get_map()["positions"]->get_vector();

		estimates.clear();

		// Loop through the positions and update them
		for (message::ptr position : positions) {
			std::string name = position->get_map()["name"]->get_string();
			std::string lat = position->get_map()["lat"]->get_string();
			std::string lon = position->get_map()["lon"]->get_string();

			OutputDebugString(name.c_str());
			OutputDebugString(" | ");
			OutputDebugString(lat.c_str());
			OutputDebugString(" | ");
			OutputDebugString(lon.c_str());
			OutputDebugString("\n");

			EuroScopePlugIn::CPosition pos;
			pos.LoadFromStrings(lon.c_str(), lat.c_str());
			estimates.push_back(std::make_tuple(name, pos));
		}
	}
	catch (...) {
		OutputDebugString("Failed to update positions.");
	}
}

struct handleData {
    unsigned long process_id;
    HWND window_handle;
};


BOOL isMainWindow(HWND handle)
{   
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enumWindowsCallback(HWND handle, LPARAM lParam)
{
	handleData& data = *(handleData*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !isMainWindow(handle))
        return TRUE;
    data.window_handle = handle;
    return FALSE;
}

HWND findMainWindow(unsigned long process_id)
{
    handleData data;
    data.process_id = process_id;
    data.window_handle = 0;
    EnumWindows(enumWindowsCallback, (LPARAM)&data);
    return data.window_handle;
}

void MessageHandler::SendPDC(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string value = e.get_message()->get_map()["value"]->get_string();

		// Init Response
		message::ptr response = object_message::create();

		bool isAssigned = false;

		// Get all windows with the specified title		
		HWND mainWindow = findMainWindow(GetCurrentProcessId());

		// If the window isn't active, but is visible (not minimized)
		// - Select the window
		// - .chat
		// - Select back

		// If minimized:
		// - Move it to a random x,y location (negative)
		// - Open it
		// - send the chat
		// - minimize and put back where it was

		SetForegroundWindow(mainWindow);
		std::string pdcTarget = ".chat " + callsign;
		MessageHandler::SendKeyboardString(pdcTarget);
		MessageHandler::SendKeyboardPresses({ ENTER });
		MessageHandler::SendKeyboardString(value);
		MessageHandler::SendKeyboardPresses({ ENTER });

		// Tell EXCDS the change is done
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (...) {
		CEXCDSBridge::SendEuroscopeMessage("PDC WARNING", "Failed to send", "UNKNOWN");
		OutputDebugString("EXCDS Error: Private message send error");
	}
}

void MessageHandler::HandoffTarget(sio::event& e)
{
	try {

		// Get aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string cjs = e.get_message()->get_map()["cjs"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
		EuroScopePlugIn::CController nextController = CEXCDSBridge::GetInstance()->ControllerSelectByPositionId(cjs.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		if (!nextController.IsValid()) {
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot handoff", "UNKNOWN");
			return;
		}

		bool isAssigned = fp.InitiateHandoff(nextController.GetCallsign());

		if (!isAssigned) {
			e.put_ack_message(NotModified(response, "Cound not handoff target."));

			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
			return;
		}

		// Tell EXCDS the change is done
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	} catch (...) {
		OutputDebugString("EXCDS Error: Handoff target error");
	}
}

void MessageHandler::RefuseHandoff(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!fp.IsValid()) {
		return;
	}

	fp.RefuseHandoff();
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Refuse handoff error");
	}
}

void MessageHandler::AcceptHandoff(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!fp.IsValid()) {
		return;
	}

	fp.AcceptHandoff();
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Accept handoff error");
	}
}

void MessageHandler::RefuseCoordination(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!fp.IsValid()) {
		return;
	}

	fp.RefuseCoordination();
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Refuse coordination error");
	}
}

void MessageHandler::AcceptCoordination(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!fp.IsValid()) {
		return;
	}

	fp.AcceptCoordination();
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Accept coordination error");
	}
}

void MessageHandler::CorrelateTarget(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string id = e.get_message()->get_map()["id"]->get_string();

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
	EuroScopePlugIn::CRadarTarget rt = CEXCDSBridge::GetInstance()->RadarTargetSelect(id.c_str());

	// Is the flight plan valid?
	if (!fp.IsValid() || !rt.IsValid()) {
		return;
	}

	fp.CorrelateWithRadarTarget(rt);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Correlate target error");
	}
}

void MessageHandler::DecorrelateTarget(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

	EuroScopePlugIn::CRadarTarget rt = CEXCDSBridge::GetInstance()->RadarTargetSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!rt.IsValid()) {
		return;
	}

	rt.Uncorrelate();
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Decorrelate target error");
	}
}

void MessageHandler::UpdateScratchPad(sio::event& e)
{
	try {

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

		bool isAssigned = false;

		try {
			isAssigned = fp.GetControllerAssignedData().SetScratchPadString(value.c_str());
		}
		catch (...) {
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify scrathpad.", "UNKNOWN");
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update scratchpad error");
	}
}

void MessageHandler::UpdateRoute(sio::event& e)
{
	try {
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

	bool isAssigned = fp.GetFlightPlanData().SetRoute(value.c_str());

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	fp.GetFlightPlanData().AmendFlightPlan();

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update route error");
	}
}

void MessageHandler::UpdateDepartureTime(sio::event& e)
{
	try {
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

	bool isAssigned = fp.GetFlightPlanData().SetEstimatedDepartureTime(value.c_str());

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	fp.GetFlightPlanData().AmendFlightPlan();

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update departure time error");
	}
}

void MessageHandler::UpdateAltitude(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string id = e.get_message()->get_map()["id"]->get_string();
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		int cleared = e.get_message()->get_map()["cleared"]->get_int();
		int final = e.get_message()->get_map()["final"]->get_int();
		int coordinated = e.get_message()->get_map()["coordinated"]->get_int();
		// Reported altitude, goes to strip annotations for situ
		std::string reported = e.get_message()->get_map()["reported"]->get_string();
		int enterCoordinated = e.get_message()->get_map()["enter"]->get_int();
		if (!enterCoordinated) enterCoordinated = -1;

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
		
		if (coordinated > 0)
			fp.InitiateCoordination(fp.GetCoordinatedNextController(), fp.GetNextCopxPointName(), coordinated);
		if (reported != "")
			fp.GetControllerAssignedData().SetFlightStripAnnotation(8, reported.c_str());
		if (enterCoordinated != -1)
			fp.InitiateCoordination(fp.GetTrackingControllerId(), fp.GetEntryCoordinationPointName(), enterCoordinated);

		fp.GetFlightPlanData().AmendFlightPlan();

		// Tell EXCDS the change is done
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (...) {
		CEXCDSBridge::SendEuroscopeMessage("ALT WARNING", "Cannot modify.", "UNKNOWN");
		OutputDebugString("EXCDS Error: Update altitude error");
	}
}

void MessageHandler::UpdateSpeed(sio::event& e)
{
	try {
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update speed error");
	}
}

void MessageHandler::UpdateFlightPlan(sio::event& e)
{
	try {
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
		std::string etd = e.get_message()->get_map()["etd"]->get_string();
		std::string route = e.get_message()->get_map()["route"]->get_string();
		std::string remarks = e.get_message()->get_map()["remarks"]->get_string();
		std::string scratchpad = e.get_message()->get_map()["scratchpad"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		// Assign data
		fp.GetFlightPlanData().SetAircraftInfo(acType.c_str());
		fp.GetFlightPlanData().SetOrigin(origin.c_str());
		fp.GetFlightPlanData().SetDestination(destination.c_str());
		fp.GetFlightPlanData().SetFinalAltitude(altitude);
		fp.GetControllerAssignedData().SetFinalAltitude(altitude);
		fp.GetFlightPlanData().SetTrueAirspeed(speed);
		fp.GetFlightPlanData().SetEnrouteHours(etehours.c_str());
		fp.GetFlightPlanData().SetEnrouteMinutes(eteminutes.c_str());
		fp.GetFlightPlanData().SetRoute(route.c_str());
		fp.GetFlightPlanData().SetEstimatedDepartureTime(etd.c_str());
		fp.GetFlightPlanData().SetRemarks(remarks.c_str());
		fp.GetControllerAssignedData().SetScratchPadString(scratchpad.c_str());

		if (strcmp(flightRules.c_str(), "I") == 0 || strcmp(flightRules.c_str(), "V") == 0)
			fp.GetFlightPlanData().SetPlanType(flightRules.c_str());


		fp.GetFlightPlanData().AmendFlightPlan();

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update Flight Plan error");
	}
}

void MessageHandler::UpdateEstimate(sio::event& e)
{
	try {
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update estimate error");
	}
}

void MessageHandler::UpdateAircraftStatus(sio::event& e)
{
	try {
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update Aircraft Status error");
	}
}

void MessageHandler::UpdateTrackingStatus(sio::event& e)
{
	try {
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update tracking status error");
	}
}

void MessageHandler::PointoutTarget(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string text = e.get_message()->get_map()["text"]->get_string();
	std::string cjs = e.get_message()->get_map()["cjs"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
	EuroScopePlugIn::CController ctrlr = CEXCDSBridge::GetInstance()->ControllerSelectByPositionId(cjs.c_str());

	if (!fp.IsValid() || !ctrlr.IsValid() || !fp.GetCorrelatedRadarTarget().IsValid()) return;

	// Get all windows with the specified title		
	HWND mainWindow = findMainWindow(GetCurrentProcessId());

	// If the window isn't active, but is visible (not minimized)
	// - Select the window
	// - .chat
	// - Select back

	// If minimized:
	// - Move it to a random x,y location (negative)
	// - Open it
	// - send the chat
	// - minimize and put back where it was

	if (!ctrlr.IsOngoingAble()) {
		std::string defaultESpout = ".point " + cjs;

		MessageHandler::SendKeyboardString(defaultESpout);

		CEXCDSBridge::GetInstance()->SetASELAircraft(fp.GetCorrelatedRadarTarget());

		MessageHandler::SendKeyboardPresses({ 0x4E });
	}

	fp.GetControllerAssignedData().SetFlightStripAnnotation(0, text.c_str());

	std::string ctrlr_cs = ctrlr.GetCallsign();

	std::string poText = "POINT OUT " + callsign + " " + text;

	SetForegroundWindow(mainWindow);
	std::string pdcTarget = ".chat " + ctrlr_cs;
	MessageHandler::SendKeyboardString(pdcTarget);
	MessageHandler::SendKeyboardPresses({ ENTER });
	MessageHandler::SendKeyboardString(poText);
	MessageHandler::SendKeyboardPresses({ ENTER });
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to pointout target");
	}
}

void MessageHandler::UpdateAnnotation(sio::event& e)
{
	try {
	// Get aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string text = e.get_message()->get_map()["text"]->get_string();
	int index = e.get_message()->get_map()["index"]->get_int();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	fp.GetControllerAssignedData().SetFlightStripAnnotation(index, text.c_str());

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update annotation error");
	}
}

void MessageHandler::UpdateSquawk(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string prefix = e.get_message()->get_map()["prefix"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		bool isAssigned;

		std::string newCode = SquawkGenerator(prefix);

		OutputDebugString(newCode.c_str());

		isAssigned = fp.GetControllerAssignedData().SetSquawk(newCode.c_str());

		if (!isAssigned) {
			e.put_ack_message(NotModified(response, "Unknown reason."));

			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
			return;
		}

		// Tell EXCDS the change is done
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update squawk error");
	}
}

void MessageHandler::UpdateDirectTo(sio::event& e)
{
	try {
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
	catch (...) {
		OutputDebugString("EXCDS Error: Update direct to error");
	}
}

void MessageHandler::UpdateTime(sio::event& e)
{
	try {
	// Gate aircraft data from EXCDS
	std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
	std::string time = e.get_message()->get_map()["time"]->get_string();

	// Init Response
	message::ptr response = object_message::create();
	response->get_map()["callsign"] = string_message::create(callsign);

	EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

	// Is the flight plan valid?
	if (!FlightPlanChecks(fp, response, e)) {
		return;
	}

	fp.GetControllerAssignedData().SetScratchPadString("DEPA");
	bool isAssigned = fp.GetFlightPlanData().SetActualDepartureTime(time.c_str());
	fp.GetControllerAssignedData().SetScratchPadString("");

	if (!isAssigned) {
		e.put_ack_message(NotModified(response, "Unknown reason."));

		CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		return;
	}

	// Tell EXCDS the change is done
	response->get_map()["modified"] = bool_message::create(true);
	e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Update time error");
	}
}

void MessageHandler::HandleNewFlightPlan(sio::event& e)
{
	try {
		// Gate aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string type = e.get_message()->get_map()["type"]->get_string();
		std::string origin = e.get_message()->get_map()["origin"]->get_string();
		std::string dest = e.get_message()->get_map()["dest"]->get_string();
		std::string route = e.get_message()->get_map()["route"]->get_string();
		std::string fpType = e.get_message()->get_map()["fpType"]->get_string();
		int alt = e.get_message()->get_map()["alt"]->get_int();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

			fp.GetFlightPlanData().SetAircraftInfo(type.c_str());
			fp.GetFlightPlanData().SetOrigin(origin.c_str());
			fp.GetFlightPlanData().SetDestination(dest.c_str());
			fp.GetFlightPlanData().SetRoute(route.c_str());
			fp.GetFlightPlanData().SetPlanType(fpType.c_str());
			fp.GetControllerAssignedData().SetClearedAltitude(0);
			fp.GetFlightPlanData().SetFinalAltitude(alt);
			fp.GetControllerAssignedData().SetFinalAltitude(alt);

			fp.GetFlightPlanData().AmendFlightPlan();

		//if (!isAssigned) {
		//	e.put_ack_message(NotModified(response, "Unknown reason."));

		//	CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
		//	return;
		//}

		// Tell EXCDS the change is done
		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to create NEW FLIGHT PLAN");
	}
}

#pragma endregion

/**
* ---------------------------
* Requesting methods
*
* For EXCDS to ask for information.
* ---------------------------
*/

#pragma region Request_Methods

void MessageHandler::RequestAirports(message::ptr response)
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	// Get a list of controllers
	EuroScopePlugIn::CController controller = bridgeInstance->ControllerSelectFirst();
	sio::message::ptr controllers = sio::array_message::create();

	while (controller.IsValid())
	{
		controllers->get_vector().push_back(string_message::create(controller.GetCallsign()));

		controller = bridgeInstance->ControllerSelectNext(controller);
	}

	// Get which airports are active
	EuroScopePlugIn::CSectorElement airport = bridgeInstance->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
	sio::message::ptr airports = sio::array_message::create();

	while (airport.IsValid())
	{
		// Check if the element is used as a departure (true) or arrival (false)
		if (airport.IsElementActive(true) || airport.IsElementActive(false))
		{
			airports->get_vector().push_back(string_message::create(airport.GetAirportName()));
		}

		airport = bridgeInstance->SectorFileElementSelectNext(airport, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
	}
}

void MessageHandler::RequestAllAircraft(sio::event& e)
{
	try {
		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
		EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

		while (flightPlan.IsValid()) {
			sio::message::ptr response = sio::object_message::create();
			MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response);

			bridgeInstance->GetSocket()->emit("SEND_ALL_FP_DATA", response);

			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
		}

		e.put_ack_message(bool_message::create(true));
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to request all aircraft");
	}
}

void MessageHandler::RequestAircraftByCallsign(sio::event& e)
{
	try{
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
catch (...) {
	OutputDebugString("EXCDS Error: Failed to request a/c by callsign");
}
}

void MessageHandler::PrepareFPTrackResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
#if _DEBUG
	char buf[100];
	struct tm newTime;
	time_t t = time(0);

	localtime_s(&newTime, &t);
	std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
	response->get_map()["timestamp"] = string_message::create(buf);
#endif
	double latitude = 0;
	double longitude = 0;

	//latitude = fp.GetFPTrackPosition().GetPosition().m_Latitude;
	//longitude = fp.GetFPTrackPosition().GetPosition().m_Longitude;
}

void MessageHandler::PrepareRadarTargetResponse(EuroScopePlugIn::CRadarTarget rt, message::ptr response)
{
#if _DEBUG
	char buf[100];
	struct tm newTime;
	time_t t = time(0);

	localtime_s(&newTime, &t);
	std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
	response->get_map()["timestamp"] = string_message::create(buf);
#endif// Radar & Position
	// PPS Enum
	// 0 - Not on Radar
	// 1 - SSR Correlated
	// 2 - SSR & PSR Correleated
	// 3 - SSR Uncorrelated
	// 4 - SSR & PSR Uncorrelated
	// 5 - Conflict / MSAW
	// 6 - Emergency
	// 7 - SSR Block
	// 8 - PSR Correlated VFR
	// 9 - Poor PSR
	// 10 - Poor SSR
	// 11 - VFR
	// 12 - RVSM
	// 13 - PSR
	// 14 - Extrapolated
	// 15 - ADSB W/O RVSM
	// 16 - ADSB RVSM
	// 17 - ADSB VFR
	// 18 - ADSB Emergency
	// 19 - ADSB Uncorrelated

	try {
		int pps = 2;
		std::string ssr = "";
		double latitude = 0;
		double longitude = 0;

		int modec = 0;
		int vs = 0;
		int groundSpeed = 0;
		bool ADSB = true;
		bool RVSM = false;
		bool RNAV = false;
		bool ident = false;
		bool VFR = false;
		bool reachedAltitude = false;
		std::string squawk = "0000";
		bool isCorrelated = false;
		std::string callsign;
		int reportedGs = 0;

		// 0 is FP track
		// 1 is Bravo (collapsed)
		// 2 is Alpha (forced open)
		int tagType = 0;

		if (rt.GetPosition().IsValid())
		{
			callsign = rt.GetCallsign();

			ssr = rt.GetPosition().GetSquawk();

			latitude = rt.GetPosition().GetPosition().m_Latitude;
			longitude = rt.GetPosition().GetPosition().m_Longitude;

			if (rt.GetPosition().GetFlightLevel() >= 18000)
				modec = (rt.GetPosition().GetFlightLevel() + 50) / 100;
			else
				modec = (rt.GetPosition().GetPressureAltitude() + 50) / 100;

			vs = rt.GetVerticalSpeed();
			reportedGs = rt.GetPosition().GetReportedGS();

			if (rt.GetPosition().GetTransponderI())
				ident = true;

			std::string transponderSquawk = rt.GetPosition().GetSquawk();
			std::string fpSquawk = "";

			if (rt.GetCorrelatedFlightPlan().IsValid())
			{
				EuroScopePlugIn::CFlightPlan fp = rt.GetCorrelatedFlightPlan();
				std::string remarks = fp.GetFlightPlanData().GetRemarks();

				isCorrelated = true;

				if (remarks.find("STS/ADSB") || fp.GetFlightPlanData().GetAircraftWtc() != 'L')
					ADSB = false;

				char capabilites = fp.GetFlightPlanData().GetCapibilities();
				switch (capabilites) {
				case 'L':
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

				VFR = strcmp(fp.GetFlightPlanData().GetPlanType(), "V") == 0;

				fpSquawk = fp.GetControllerAssignedData().GetSquawk();
			}
		}

		bool MEDEVAC = false;
		std::string wt = "";
		std::string reportedAltitude = "";
		std::string clearedAltitude = "";
		bool altitudeError = false;
		int finalAltitude = 0;
		std::string hocjs = "";
		std::string assignedSpeed = "";
		int estimatedIas = 0;
		int estimatedMach = 0;
		std::string cjs = "";
		std::string acType = "";
		std::string destination = "";
		std::string depRwy = "";
		std::string arrRwy = "";
		std::string origin = "";
		int eta = 0;
		int distanceToDestination = 0;
		std::string sfi = "";
		bool isTrackedByMe = false;
		bool hoBlink = false;
		bool isVfr = false;
		bool ram = false;
		sio::message::ptr pointsMessage = sio::array_message::create();
		int assignedHeading = 0;
		int trackingState = EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED;
		bool cleared = false;
		std::string groundStatus = "NSTS";
		std::string nextCjs = "";
		std::string assignedSquawk = "";
		std::string etd = "";
		std::string atd = "";
		std::string route = "";
		std::string remarks = "";
		std::string filedSpeed = "";
		int sectorEntryTime = 0;
		int sectorExitTime = 0;
		std::string commType = "";
		double frequency = 199.998;

		std::string ann1 = "";
		std::string ann2 = "";
		std::string ann3 = "";
		std::string ann4 = "";
		std::string ann5 = "";
		std::string ann6 = "";
		std::string ann7 = "";
		std::string ann8 = "";
		std::string ann9 = "";

		if (rt.GetCorrelatedFlightPlan().IsValid())
		{
			EuroScopePlugIn::CFlightPlan fp = rt.GetCorrelatedFlightPlan();
			std::string remarks = fp.GetFlightPlanData().GetRemarks();
			CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

			ram = fp.GetRAMFlag();

			cjs = fp.GetTrackingControllerId();

			EuroScopePlugIn::CController trackingController = bridgeInstance->ControllerSelectByPositionId(fp.GetTrackingControllerId());
			frequency = trackingController.GetPrimaryFrequency();

			EuroScopePlugIn::CController nextController = bridgeInstance->ControllerSelect(fp.GetCoordinatedNextController());
			if (nextController.IsValid())
			{
				nextCjs = nextController.GetPositionId();
			}

			if (fp.GetSectorExitMinutes() < 3 && fp.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_ASSUMED)
			{
				hoBlink = true;
			}

			remarks = fp.GetFlightPlanData().GetRemarks();

			if (remarks.find("STS/MEDEVAC") != std::string::npos)
				MEDEVAC = true;

			//test

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
			else if (tempAlt == 0)
			{
				tempAlt = fp.GetFinalAltitude();
				clearedAltitude = "C" + std::to_string(fp.GetFinalAltitude() / 100);
			}
			else if (tempAlt > 0)
				clearedAltitude = "C" + std::to_string(tempAlt / 100);
			else
				clearedAltitude = "Cclr";

			int alt = modec * 100;

			if (alt > tempAlt + 200 || alt < tempAlt - 200) {
				if (rt.GetVerticalSpeed() < 200 && rt.GetVerticalSpeed() > -200)
					altitudeError = true;
				if (alt < tempAlt - 200 && rt.GetVerticalSpeed() < -200)
					altitudeError = true;
				if (alt > tempAlt + 200 && rt.GetVerticalSpeed() > 200)
					altitudeError = true;
			}
			else
				reachedAltitude = true;

			finalAltitude = fp.GetFlightPlanData().GetFinalAltitude() / 100;

			hocjs = fp.GetHandoffTargetControllerId();

			if (fp.GetControllerAssignedData().GetAssignedMach() > 0) {
				double val = fp.GetControllerAssignedData().GetAssignedMach();
				val = val / 100;
				assignedSpeed = "A" + std::to_string(val);
			}
			else if (fp.GetControllerAssignedData().GetAssignedSpeed() > 0)
				assignedSpeed = "A" + std::to_string(fp.GetControllerAssignedData().GetAssignedSpeed());

			if (rt.GetPosition().IsValid())
			{
				estimatedIas = fp.GetFlightPlanData().PerformanceGetIas(rt.GetPosition().GetPressureAltitude(), 0);
				estimatedMach = fp.GetFlightPlanData().PerformanceGetMach(rt.GetPosition().GetFlightLevel(), 0);
			}

			acType = fp.GetFlightPlanData().GetAircraftFPType();
			destination = fp.GetFlightPlanData().GetDestination();
			origin = fp.GetFlightPlanData().GetOrigin();

			depRwy = fp.GetFlightPlanData().GetDepartureRwy();
			arrRwy = fp.GetFlightPlanData().GetArrivalRwy();

			eta = fp.GetPositionPredictions().GetPointsNumber();
			distanceToDestination = fp.GetDistanceToDestination();

			switch (fp.GetControllerAssignedData().GetCommunicationType())
			{
			case 'R':
			case 'r':
				commType = "R";
				break;
			case 't':
			case 'T':
				commType = "T";
				break;
			default:
				commType = "V";
			}

			sfi = fp.GetControllerAssignedData().GetScratchPadString();

			const char* flightRules = fp.GetFlightPlanData().GetPlanType();
			if (strcmp(flightRules, "V") == 0) isVfr = true;

			trackingState = fp.GetState();

			cleared = fp.GetClearenceFlag();
			groundStatus = fp.GetGroundState();

			assignedSquawk = fp.GetControllerAssignedData().GetSquawk();

			etd = fp.GetFlightPlanData().GetEstimatedDepartureTime();
			atd = fp.GetFlightPlanData().GetActualDepartureTime();

			for (int i = 0; i < fp.GetExtractedRoute().GetPointsNumber(); i++) {
				sio::message::ptr msg = sio::object_message::create();

				EuroScopePlugIn::CPosition pos = fp.GetExtractedRoute().GetPointPosition(i);
				msg->get_map()["lat"] = double_message::create(pos.m_Latitude);
				msg->get_map()["long"] = double_message::create(pos.m_Longitude);

				msg->get_map()["name"] = string_message::create(fp.GetExtractedRoute().GetPointName(i));
				msg->get_map()["eta"] = int_message::create(fp.GetExtractedRoute().GetPointDistanceInMinutes(i));

				pointsMessage->get_vector().push_back(msg);
			}

			assignedHeading = fp.GetControllerAssignedData().GetAssignedHeading();

			route = fp.GetFlightPlanData().GetRoute();

			filedSpeed = fp.GetFlightPlanData().GetTrueAirspeed();

			sectorEntryTime = fp.GetSectorEntryMinutes();
			sectorExitTime = fp.GetSectorExitMinutes();

			ann1 = fp.GetControllerAssignedData().GetFlightStripAnnotation(0);
			ann2 = fp.GetControllerAssignedData().GetFlightStripAnnotation(1);
			ann3 = fp.GetControllerAssignedData().GetFlightStripAnnotation(2);
			ann4 = fp.GetControllerAssignedData().GetFlightStripAnnotation(3);
			ann5 = fp.GetControllerAssignedData().GetFlightStripAnnotation(4);
			ann6 = fp.GetControllerAssignedData().GetFlightStripAnnotation(5);
			ann7 = fp.GetControllerAssignedData().GetFlightStripAnnotation(6);
			ann8 = fp.GetControllerAssignedData().GetFlightStripAnnotation(7);
			ann9 = fp.GetControllerAssignedData().GetFlightStripAnnotation(8);
		}

		response->get_map()["radar"] = object_message::create();
		response->get_map()["radar"]->get_map()["ssr"] = string_message::create(ssr);
		response->get_map()["radar"]->get_map()["altitude"] = int_message::create(modec);
		response->get_map()["radar"]->get_map()["vertical_speed"] = int_message::create(vs);
		response->get_map()["radar"]->get_map()["ground_speed"] = int_message::create(rt.GetGS());
		response->get_map()["radar"]->get_map()["pps"] = int_message::create(pps);
		response->get_map()["radar"]->get_map()["radar_flags"] = int_message::create(rt.GetPosition().GetRadarFlags());
		response->get_map()["radar"]->get_map()["track"] = int_message::create(rt.GetPosition().GetReportedHeadingTrueNorth());

		response->get_map()["position"] = object_message::create();
		response->get_map()["position"]->get_map()["lat"] = double_message::create(latitude);
		response->get_map()["position"]->get_map()["long"] = double_message::create(longitude);

		response->get_map()["internal"] = object_message::create();
		response->get_map()["internal"]->get_map()["reported_gs"] = int_message::create(reportedGs);
		response->get_map()["internal"]->get_map()["controller_tracking_state"] = int_message::create(trackingState);
		response->get_map()["internal"]->get_map()["clearance_flag"] = bool_message::create(cleared);
		response->get_map()["internal"]->get_map()["ground_status"] = string_message::create(groundStatus);
		response->get_map()["internal"]->get_map()["next_cjs"] = string_message::create(nextCjs);
		response->get_map()["internal"]->get_map()["assignedSquawk"] = string_message::create(assignedSquawk);
		response->get_map()["internal"]->get_map()["eta"] = int_message::create(eta);
		response->get_map()["internal"]->get_map()["etd"] = string_message::create(etd);
		response->get_map()["internal"]->get_map()["atd"] = string_message::create(atd);
		response->get_map()["internal"]->get_map()["sector_entry_time"] = int_message::create(sectorEntryTime);
		response->get_map()["internal"]->get_map()["sector_exit_time"] = int_message::create(sectorExitTime);
		response->get_map()["internal"]->get_map()["frequency"] = double_message::create(frequency);

		response->get_map()["annotations"] = object_message::create();
		response->get_map()["annotations"]->get_map()["1"] = string_message::create(ann1);
		response->get_map()["annotations"]->get_map()["2"] = string_message::create(ann2);
		response->get_map()["annotations"]->get_map()["3"] = string_message::create(ann3);
		response->get_map()["annotations"]->get_map()["4"] = string_message::create(ann4);
		response->get_map()["annotations"]->get_map()["5"] = string_message::create(ann5);
		response->get_map()["annotations"]->get_map()["6"] = string_message::create(ann6);
		response->get_map()["annotations"]->get_map()["7"] = string_message::create(ann7);
		response->get_map()["annotations"]->get_map()["8"] = string_message::create(ann8);
		response->get_map()["annotations"]->get_map()["9"] = string_message::create(ann9);

		response->get_map()["mods"] = object_message::create();
		response->get_map()["mods"]->get_map()["medevac"] = bool_message::create(MEDEVAC);
		response->get_map()["mods"]->get_map()["rvsm"] = bool_message::create(RVSM);
		response->get_map()["mods"]->get_map()["adsb"] = bool_message::create(ADSB);
		response->get_map()["mods"]->get_map()["altitude_error"] = bool_message::create(altitudeError);
		response->get_map()["mods"]->get_map()["rnav"] = bool_message::create(RNAV);
		response->get_map()["mods"]->get_map()["text"] = string_message::create(commType);
		response->get_map()["mods"]->get_map()["reached_altitude"] = bool_message::create(reachedAltitude);
		response->get_map()["mods"]->get_map()["blink"] = bool_message::create(hoBlink);
		response->get_map()["mods"]->get_map()["vfr"] = bool_message::create(isVfr);
		response->get_map()["mods"]->get_map()["ident"] = bool_message::create(ident);
		response->get_map()["mods"]->get_map()["correlated"] = bool_message::create(isCorrelated);
		response->get_map()["mods"]->get_map()["trackedByMe"] = bool_message::create(isTrackedByMe);
		response->get_map()["mods"]->get_map()["ram"] = bool_message::create(ram);

		response->get_map()["general"] = object_message::create();
		response->get_map()["general"]->get_map()["callsign"] = string_message::create(callsign);
		response->get_map()["general"]->get_map()["wtc"] = string_message::create(wt);
		response->get_map()["general"]->get_map()["handoff_cjs"] = string_message::create(hocjs);
		response->get_map()["general"]->get_map()["cjs"] = string_message::create(cjs);
		response->get_map()["general"]->get_map()["ac_type"] = string_message::create(acType);
		response->get_map()["general"]->get_map()["destination"] = string_message::create(destination);
		response->get_map()["general"]->get_map()["origin"] = string_message::create(origin);
		response->get_map()["general"]->get_map()["type"] = int_message::create(tagType);
		response->get_map()["general"]->get_map()["sfi"] = string_message::create(sfi);
		response->get_map()["general"]->get_map()["origin_rwy"] = string_message::create(depRwy);
		response->get_map()["general"]->get_map()["dest_rwy"] = string_message::create(arrRwy);
		response->get_map()["general"]->get_map()["distance"] = int_message::create(distanceToDestination);
		response->get_map()["general"]->get_map()["heading"] = int_message::create(assignedHeading);
		response->get_map()["general"]->get_map()["route"] = string_message::create(route);
		response->get_map()["general"]->get_map()["remarks"] = string_message::create(remarks);

		response->get_map()["altitude"] = object_message::create();
		response->get_map()["altitude"]->get_map()["reported"] = string_message::create(reportedAltitude);
		response->get_map()["altitude"]->get_map()["cleared"] = string_message::create(clearedAltitude);
		response->get_map()["altitude"]->get_map()["filed"] = int_message::create(finalAltitude);

		response->get_map()["speed"] = object_message::create();
		response->get_map()["speed"]->get_map()["estimated_mach"] = int_message::create(estimatedMach);
		response->get_map()["speed"]->get_map()["estimated_speed"] = int_message::create(estimatedIas);
		response->get_map()["speed"]->get_map()["filed"] = int_message::create(estimatedIas);
		response->get_map()["speed"]->get_map()["assigned"] = string_message::create(assignedSpeed);

		response->get_map()["points"] = pointsMessage;

		response->get_map()["id"] = string_message::create(rt.GetSystemID());
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed radar target update");
	}
}

void MessageHandler::PrepareRouteDataResponse(sio::event& e)
{
	try {
		// Gate data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

		EuroScopePlugIn::CFlightPlan fp = bridgeInstance->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!fp.IsValid()) {
			return;
		}

		sio::message::ptr arrayMessage = sio::array_message::create();

		int directTo = fp.GetExtractedRoute().GetPointsAssignedIndex();
		int closestTo = fp.GetExtractedRoute().GetPointsCalculatedIndex();
		int highest = 0;
		if (directTo > closestTo)
			highest = directTo;
		else
			highest = closestTo;

		for (int i = highest; i < fp.GetExtractedRoute().GetPointsNumber(); i++) {
			double lat = fp.GetExtractedRoute().GetPointPosition(i).m_Latitude;
			double lon = fp.GetExtractedRoute().GetPointPosition(i).m_Longitude;

			sio::message::ptr msg = sio::object_message::create();
			msg->get_map()["point"] = string_message::create(fp.GetExtractedRoute().GetPointName(i));
			msg->get_map()["lat"] = double_message::create(lat);
			msg->get_map()["long"] = double_message::create(lon);

			arrayMessage->get_vector().push_back(msg);

			if (fp.GetExtractedRoute().GetPointDistanceInMinutes(i) > fp.GetSectorExitMinutes())
				break;
		}

		double lat = 0;
		double lon = 0;

		if (fp.GetCorrelatedRadarTarget().IsValid())
		{
			lat = fp.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Latitude;
			lon = fp.GetCorrelatedRadarTarget().GetPosition().GetPosition().m_Longitude;
		}
		else if (fp.GetFPTrackPosition().IsValid())
		{
			lat = fp.GetFPTrackPosition().GetPosition().m_Latitude;
			lon = fp.GetFPTrackPosition().GetPosition().m_Longitude;
		}

		response->get_map()["lat"] = double_message::create(lat);
		response->get_map()["long"] = double_message::create(lon);

		response->get_map()["route"] = arrayMessage;
		bridgeInstance->GetSocket()->emit("ROUTE_DATA", response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: cannot get route data");
	}
}

void MessageHandler::PrepareCDMResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
	if (!fp.IsValid()) return;

	if (strcmp(fp.GetFlightPlanData().GetOrigin(), "CYYZ") != 0 || strcmp(fp.GetFlightPlanData().GetDestination(), "CYYZ") != 0) return;

	std::string callsign = fp.GetCallsign();
	response->get_map()["callsign"] = string_message::create(callsign);

	response->get_map()["etd"] = string_message::create(fp.GetFlightPlanData().GetEstimatedDepartureTime());
	response->get_map()["remarks"] = string_message::create(fp.GetFlightPlanData().GetRemarks());
	response->get_map()["ete"] = int_message::create(fp.GetPositionPredictions().GetPointsNumber());
	response->get_map()["hours"] = string_message::create(fp.GetFlightPlanData().GetEnrouteHours());
	response->get_map()["minutes"] = string_message::create(fp.GetFlightPlanData().GetEnrouteMinutes());
	response->get_map()["flight_plan_state"] = int_message::create(fp.GetFPState());
}

void MessageHandler::PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response)
{
	if (!fp.IsValid()) {
		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error: Flight plan not valid", "FP INVALID");
		return;
	}

	char buf[100];
	struct tm newTime;
	time_t t = time(0);

	localtime_s(&newTime, &t);
	std::strftime(buf, 100, "%Y-%m-%d %H:%M:%S", &newTime);
	response->get_map()["timestamp"] = string_message::create(buf);

	try {
	// Aircraft
		std::string acType = fp.GetFlightPlanData().GetAircraftFPType();
		char wakeTurbulenceCat = fp.GetFlightPlanData().GetAircraftWtc();
		char equip = fp.GetFlightPlanData().GetCapibilities();
		std::string typeString = "/" + acType + "/";

		switch (equip) {
		case 'Q': case 'W': case 'L':
			typeString += "W";
			break;
		case 'R':
			typeString += "R";
			break;
		case 'G': case 'Y': case 'C': case 'I':
			typeString += "G";
			break;
		case 'E':case 'F':
			typeString += "E";
			break;
		case 'A': case 'T':
			typeString += "S";
			break;
		default:
			typeString += "N";
		}

		typeString.insert(0, 1, wakeTurbulenceCat);

		std::string equipment = fp.GetFlightPlanData().GetAircraftFPType();
		int equipmentIndex = equipment.find_last_of('/');
		if (equipmentIndex > 0)
			equipment = equipment.substr(equipment.find_last_of('/'));
		else
			equipment = equip;

		std::string wtcat = "";
		switch (fp.GetFlightPlanData().GetAircraftWtc()) {
		case 'L':
			wtcat = "L";
			break;
		case 'M':
			wtcat = "M";
			break;
		case 'H':
			wtcat = "H";
			break;
		case 'J':
			wtcat = "J";
			break;
		default:
			wtcat = "?";
		}

		std::string engine = "";

		switch (fp.GetFlightPlanData().GetEngineType()) {
		case 'J':
			engine = "J";
			break;
		case 'T':
			engine = "T";
			break;
		default:
			engine = "P";
		}

		response->get_map()["aircraft"] = object_message::create();
		response->get_map()["aircraft"]->get_map()["abbr"] = string_message::create(typeString);
		response->get_map()["aircraft"]->get_map()["engine"] = string_message::create(engine);
		response->get_map()["aircraft"]->get_map()["equip"] = string_message::create(equipment);
		response->get_map()["aircraft"]->get_map()["icao"] = string_message::create(acType);
		response->get_map()["aircraft"]->get_map()["wtcat"] = string_message::create(wtcat);
		response->get_map()["aircraft"]->get_map()["nav"] = string_message::create(fp.GetFlightPlanData().GetAircraftInfo());

		// Annotations
		std::string ann1 = fp.GetControllerAssignedData().GetFlightStripAnnotation(0);
		std::string ann2 = fp.GetControllerAssignedData().GetFlightStripAnnotation(1);
		std::string ann3 = fp.GetControllerAssignedData().GetFlightStripAnnotation(2);
		std::string ann4 = fp.GetControllerAssignedData().GetFlightStripAnnotation(3);
		std::string ann5 = fp.GetControllerAssignedData().GetFlightStripAnnotation(4);
		std::string ann6 = fp.GetControllerAssignedData().GetFlightStripAnnotation(5);
		std::string ann7 = fp.GetControllerAssignedData().GetFlightStripAnnotation(6);
		std::string ann8 = fp.GetControllerAssignedData().GetFlightStripAnnotation(7);
		std::string ann9 = fp.GetControllerAssignedData().GetFlightStripAnnotation(8);

		response->get_map()["annotations"] = object_message::create();
		response->get_map()["annotations"]->get_map()["1"] = string_message::create(ann1);
		response->get_map()["annotations"]->get_map()["2"] = string_message::create(ann2);
		response->get_map()["annotations"]->get_map()["3"] = string_message::create(ann3);
		response->get_map()["annotations"]->get_map()["4"] = string_message::create(ann4);
		response->get_map()["annotations"]->get_map()["5"] = string_message::create(ann5);
		response->get_map()["annotations"]->get_map()["6"] = string_message::create(ann6);
		response->get_map()["annotations"]->get_map()["7"] = string_message::create(ann7);
		response->get_map()["annotations"]->get_map()["8"] = string_message::create(ann8);
		response->get_map()["annotations"]->get_map()["9"] = string_message::create(ann9);

		// Altitude Information
		int coordinated = fp.GetExitCoordinationAltitude();
		int final = fp.GetControllerAssignedData().GetFinalAltitude();
		if (final == 0)
			final = fp.GetFlightPlanData().GetFinalAltitude();
		if (coordinated == 0)
			coordinated = final;
		int entry = fp.GetEntryCoordinationAltitude();

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
		else if (clearedAlt == 1 || clearedAlt == 2)
			clearedAltString = "CAPR"; // Cleared for an approach
		else if (clearedAlt == 3)
			clearedAltString = "B"; // B is used to indicate an aircraft has been cleared out of (high level) controlled airspace
		else
			clearedAltString = std::to_string(clearedAlt / 100);

		response->get_map()["altitude"] = object_message::create();
		response->get_map()["altitude"]->get_map()["cleared"] = object_message::create();
		response->get_map()["altitude"]->get_map()["cleared"]->get_map()["abbr"] = string_message::create(clearedAltString);
		response->get_map()["altitude"]->get_map()["cleared"]->get_map()["value"] = int_message::create(clearedAlt);
		response->get_map()["altitude"]->get_map()["coordinated"] = int_message::create(coordinated);
		response->get_map()["altitude"]->get_map()["entry"] = int_message::create(entry);
		response->get_map()["altitude"]->get_map()["final"] = object_message::create();
		response->get_map()["altitude"]->get_map()["final"]->get_map()["abbr"] = string_message::create(filedAltString);
		response->get_map()["altitude"]->get_map()["final"]->get_map()["value"] = int_message::create(final);
		response->get_map()["altitude"]->get_map()["reported"] = string_message::create("");

		// Callsign
		response->get_map()["callsign"] = string_message::create(fp.GetCallsign());

		// Controller Assigned Data
		std::string excdsGroundStatus = "";

		const char* groundStatus = fp.GetGroundState();
		bool cleared = fp.GetClearenceFlag();
		if (strcmp(groundStatus, "PUSH") == 0)
			excdsGroundStatus = "PUSH";
		else if (strcmp(groundStatus, "ARR") == 0)
			excdsGroundStatus = "ARR";
		else if (strcmp(groundStatus, "TXIN") == 0)
			excdsGroundStatus = "TXIN";
		else if (strcmp(groundStatus, "PARK") == 0)
			excdsGroundStatus = "PARK";
		else if (strcmp(groundStatus, "TAXI") == 0)
		{
			if (strcmp(fp.GetControllerAssignedData().GetScratchPadString(), "RREL") == 0)
				excdsGroundStatus = "TXRL";
			else if (strcmp(fp.GetControllerAssignedData().GetScratchPadString(), "RREQ") == 0)
				excdsGroundStatus = "TXRQ";
			else
				excdsGroundStatus = "TAXI";
		}
		else
		{
			if (cleared)
			{
				if (strcmp(groundStatus, "DEPA") == 0)
					excdsGroundStatus = "DEPA";
				else
					excdsGroundStatus = "CLEA";
			}
			else
			{
				excdsGroundStatus = "NSTS";
			}
		}

		// Special Status
		// T = Text
		// M = Medevac
		// R = Recieve only
		// ? = Unknown voice capability

		std::string excdsStatus;
		std::string remarks = fp.GetFlightPlanData().GetRemarks();
		char commType = std::toupper(fp.GetControllerAssignedData().GetCommunicationType());

		if (commType == 'T')
			excdsStatus = "T";
		else if (remarks.find("STS/MEDEVAC") != std::string::npos)
			excdsStatus = "M";
		else if (commType == 'R')
			excdsStatus = "R";
		else if (commType == '?')
			excdsStatus = "?";
		else
			excdsStatus = "";

		int fpstate = fp.GetState();
		if (fpstate == 2 && fp.GetSectorEntryMinutes() > 5)
			fpstate = 1;

		response->get_map()["controllerData"] = object_message::create();
		response->get_map()["controllerData"]->get_map()["heading"] = int_message::create(fp.GetControllerAssignedData().GetAssignedHeading());
		response->get_map()["controllerData"]->get_map()["ground_status"] = string_message::create(excdsGroundStatus);
		response->get_map()["controllerData"]->get_map()["scratchpad"] = string_message::create(fp.GetControllerAssignedData().GetScratchPadString());
		response->get_map()["controllerData"]->get_map()["special_status"] = string_message::create(excdsStatus);
		response->get_map()["controllerData"]->get_map()["fp_tracking_state"] = int_message::create(fp.GetFPState());
		response->get_map()["controllerData"]->get_map()["squawk"] = string_message::create(fp.GetControllerAssignedData().GetSquawk());
		response->get_map()["controllerData"]->get_map()["controller_tracking_state"] = int_message::create(fpstate);

		try {
			CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
			EuroScopePlugIn::CController trackingController = bridgeInstance->ControllerSelectByPositionId(fp.GetTrackingControllerId());

			EuroScopePlugIn::CController nextController = bridgeInstance->ControllerSelect(fp.GetCoordinatedNextController());
			std::string nextCtrlr = "";
			if (nextController.IsValid()) nextCtrlr = nextController.GetPositionId();
			double frequency = 199.998;
			if (trackingController.IsValid())
				frequency = trackingController.GetPrimaryFrequency();
			response->get_map()["controllerData"]->get_map()["next_controller"] = string_message::create(nextCtrlr);
			response->get_map()["controllerData"]->get_map()["freq"] = double_message::create(frequency);
			response->get_map()["controllerData"]->get_map()["tracking_controller"] = string_message::create(trackingController.GetPositionId());
		}
		catch (...) {
			OutputDebugString("EXCDS error getting controller data");
		}

		// Flight Plan Data
		response->get_map()["fpdata"] = object_message::create();
		response->get_map()["fpdata"]->get_map()["alerting"] = bool_message::create(fp.GetClearenceFlag());
		response->get_map()["fpdata"]->get_map()["ifr"] = bool_message::create(strcmp(fp.GetFlightPlanData().GetPlanType(), "I") == 0);
		response->get_map()["fpdata"]->get_map()["remarks"] = string_message::create(ApiHelper::ToASCII(fp.GetFlightPlanData().GetRemarks()));

		// FP Track
		response->get_map()["fp_track"] = object_message::create();

		bool isValid = fp.GetFPState() == 1 && !fp.GetCorrelatedRadarTarget().IsValid() && fp.GetFPTrackPosition().IsValid();
		response->get_map()["fp_track"]->get_map()["is_valid"] = bool_message::create(isValid);

		if (isValid) {
			response->get_map()["fp_track"]->get_map()["lat"] = double_message::create(fp.GetFPTrackPosition().GetPosition().m_Latitude);
			response->get_map()["fp_track"]->get_map()["long"] = double_message::create(fp.GetFPTrackPosition().GetPosition().m_Longitude);
			response->get_map()["fp_track"]->get_map()["track"] = double_message::create(fp.GetFPTrackPosition().GetReportedHeadingTrueNorth());
		}


		if (fp.GetState() > 0) {
			// Route Information
			EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
			EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

			boolean eastbound = true;
			if (origin.DirectionTo(destination) > 179)
				eastbound = false;

			std::string dep = fp.GetFlightPlanData().GetOrigin();
			std::string depRwy = fp.GetFlightPlanData().GetDepartureRwy();
			std::string sid = fp.GetFlightPlanData().GetSidName();

			std::string dest = fp.GetFlightPlanData().GetDestination();
			std::string arrRwy = fp.GetFlightPlanData().GetArrivalRwy();
			std::string star = fp.GetFlightPlanData().GetStarName();

			std::string route = fp.GetFlightPlanData().GetRoute();

			std::string first = fp.GetExtractedRoute().GetPointName(1);

			EuroScopePlugIn::CPosition sectorExit = fp.GetPositionPredictions().GetPosition(fp.GetSectorExitMinutes());
			double sectorExitLat = sectorExit.m_Latitude;
			double sectorExitLong = sectorExit.m_Longitude;

			int sectorEntryTime = fp.GetSectorEntryMinutes();
			int sectorExitTime = fp.GetSectorExitMinutes();

			response->get_map()["route"] = object_message::create();
			response->get_map()["route"]->get_map()["departure"] = object_message::create();
			response->get_map()["route"]->get_map()["destination"] = object_message::create();

			response->get_map()["route"]->get_map()["departure"]->get_map()["code"] = string_message::create(dep);
			response->get_map()["route"]->get_map()["departure"]->get_map()["rwy"] = string_message::create(depRwy);
			response->get_map()["route"]->get_map()["departure"]->get_map()["procedure"] = string_message::create(ApiHelper::ToASCII(sid));
			response->get_map()["route"]->get_map()["departure"]->get_map()["distance"] = double_message::create(fp.GetDistanceFromOrigin());
			response->get_map()["route"]->get_map()["destination"]->get_map()["code"] = string_message::create(dest);
			response->get_map()["route"]->get_map()["destination"]->get_map()["rwy"] = string_message::create(arrRwy);
			response->get_map()["route"]->get_map()["destination"]->get_map()["procedure"] = string_message::create(ApiHelper::ToASCII(star));
			response->get_map()["route"]->get_map()["destination"]->get_map()["distance"] = double_message::create(fp.GetDistanceToDestination());
			response->get_map()["route"]->get_map()["eastbound"] = bool_message::create(eastbound);
			response->get_map()["route"]->get_map()["text"] = string_message::create(ApiHelper::ToASCII(route));
			response->get_map()["route"]->get_map()["first_fix"] = string_message::create(ApiHelper::ToASCII(first));
			response->get_map()["route"]->get_map()["track"] = int_message::create(origin.DirectionTo(destination));
			response->get_map()["route"]->get_map()["sector_exit_lat"] = double_message::create(sectorExitLat);
			response->get_map()["route"]->get_map()["sector_exit_lon"] = double_message::create(sectorExitLong);
			response->get_map()["route"]->get_map()["sector_entry_time"] = int_message::create(sectorEntryTime);
			response->get_map()["route"]->get_map()["sector_exit_time"] = int_message::create(sectorExitTime);
		}

		if (fp.GetFPTrackPosition().IsValid())
		{
			sio::message::ptr pointsMessage = sio::array_message::create();

			for (int i = 0; i < fp.GetExtractedRoute().GetPointsNumber(); i++) {
				sio::message::ptr msg = sio::object_message::create();

				EuroScopePlugIn::CPosition pos = fp.GetExtractedRoute().GetPointPosition(i);
				msg->get_map()["lat"] = double_message::create(pos.m_Latitude);
				msg->get_map()["long"] = double_message::create(pos.m_Longitude);

				msg->get_map()["name"] = string_message::create(fp.GetExtractedRoute().GetPointName(i));

				pointsMessage->get_vector().push_back(msg);
			}

			response->get_map()["points"] = pointsMessage;
		}

		// Speeds
		std::string speed;
		float assignedMach = fp.GetControllerAssignedData().GetAssignedMach() / 100;
		int assignedSpeed = fp.GetControllerAssignedData().GetAssignedSpeed();

		response->get_map()["speeds"] = object_message::create();
		response->get_map()["speeds"]->get_map()["abbr"] = int_message::create(fp.GetFlightPlanData().GetTrueAirspeed());
		response->get_map()["speeds"]->get_map()["assigned_mach"] = int_message::create(assignedMach);
		response->get_map()["speeds"]->get_map()["assigned_speed"] = int_message::create(assignedSpeed);

		// Times
		std::string enrouteHours = fp.GetFlightPlanData().GetEnrouteHours();
		std::string enrouteMinutes = fp.GetFlightPlanData().GetEnrouteMinutes();

		response->get_map()["times"] = object_message::create();
		response->get_map()["times"]->get_map()["actual"] = string_message::create(fp.GetFlightPlanData().GetActualDepartureTime());
		response->get_map()["times"]->get_map()["enroute_mins"] = string_message::create(enrouteMinutes);
		response->get_map()["times"]->get_map()["enroute_hours"] = string_message::create(enrouteHours);
		response->get_map()["times"]->get_map()["departure"] = string_message::create(fp.GetFlightPlanData().GetEstimatedDepartureTime());
		response->get_map()["times"]->get_map()["ete"] = int_message::create(fp.GetPositionPredictions().GetPointsNumber());

		response->get_map()["success"] = bool_message::create(true);

		response->get_map()["estimates"] = object_message::create();

		if (fp.GetState() > 1 && fp.GetFPState() == 1)
		{
			EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
			EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

			// EXCDs estimate
			std::string arrivalEstimateName = "";
			int arrivalEstimateTime = -1;

			arrivalEstimateName = fp.GetFlightPlanData().GetDestination();
			arrivalEstimateTime = fp.GetPositionPredictions().GetPointsNumber() - 1;

			response->get_map()["estimates"]->get_map()["arrival_time"] = int_message::create(arrivalEstimateTime);
			response->get_map()["estimates"]->get_map()["arrival_fix"] = string_message::create(arrivalEstimateName);

			if (strcmp(fp.GetFlightPlanData().GetPlanType(), "I") == 0) {
				response->get_map()["estimates"]->get_map()["enroute"] = object_message::create();
				int closestBayDistance = 1000;
				//int closestBayTime = 500;
				std::string closestBayName = "";

				for (int i = 0; i < estimates.size(); i++)
				{
					std::tuple <std::string, EuroScopePlugIn::CPosition> fix = estimates[i];
					EuroScopePlugIn::CPosition posn = std::get<1>(fix);

					double distance = fp.GetPositionPredictions().GetPosition(0).DistanceTo(posn);

					int upperBound = fp.GetPositionPredictions().GetPointsNumber();

					if (upperBound > 120)
						upperBound = 120;

					for (int j = 1; j <= upperBound; j++)
					{
						// If we are getting closer to the fix
						if (distance > fp.GetPositionPredictions().GetPosition(j).DistanceTo(posn))
						{
							distance = fp.GetPositionPredictions().GetPosition(j).DistanceTo(posn);
						}
						// Do not include if we are further than 200 miles away from the fix
						else if (j == 1 || fp.GetPositionPredictions().GetPosition(j).DistanceTo(posn) > 300)
						{
							break;
						}
						else
						{
							std::string fixName = std::get<0>(fix);
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName] = object_message::create();
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["name"] = string_message::create(fixName);
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["ete"] = int_message::create(j - 1);

							double displacement = sqrt(pow(distance, 2) + pow(fp.GetPositionPredictions().GetPosition(0).DistanceTo(posn), 2));
							double distanceToAbeam = fp.GetPositionPredictions().GetPosition(j).DistanceTo(fp.GetPositionPredictions().GetPosition(0));

							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["current_distance"] = double_message::create(displacement);
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["abeam_distance"] = double_message::create(distance);
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["distance_to_abeam"] = double_message::create(distanceToAbeam);

							EuroScopePlugIn::CPosition abeamPoint = fp.GetPositionPredictions().GetPosition(j - 1);

							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["lat"] = double_message::create(abeamPoint.m_Latitude);
							response->get_map()["estimates"]->get_map()["enroute"]->get_map()[fixName]->get_map()["lon"] = double_message::create(abeamPoint.m_Longitude);

							if (displacement < closestBayDistance)
							{
								closestBayDistance = displacement;
								closestBayName = fixName;
							}

							break;
						}
					}
				}

				if (closestBayName.empty())
				{
					for (int i = 0; i < estimates.size(); i++)
					{
						std::tuple <std::string, EuroScopePlugIn::CPosition> fix = estimates[i];
						EuroScopePlugIn::CPosition posn = std::get<1>(fix);

						double distance = origin.DistanceTo(posn);

						if (distance < closestBayDistance || closestBayDistance == 1000)
						{
							closestBayDistance = distance;
							closestBayName = std::get<0>(fix);
						}
					}
				}

				response->get_map()["estimates"]->get_map()["closest_bay"] = string_message::create(closestBayName);
			}
		}
		response->get_map()["reason"] = string_message::create("Error occured while generating estimates.");
		response->get_map()["success"] = bool_message::create(false);

		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error occured while generating estimates.", "CANT_GET_DATA");
	}
	catch (...) {
		CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Error: Flight plan not valid", "FP INVALID");
		OutputDebugString("EXCDS Error: Problem with fp data aqcuisition");
	}
}

#pragma endregion

/**
* ---------------------------
* Internal methods
*
* Methods only used for this class!
* ---------------------------
*/

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
}

/*
* This is used to check if a callsign has a flight plan to modify, and we (the controller) are able to modify it.
* Returns true if the flight plan is valid.
*/
bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, message::ptr response, sio::event& e)
{
	//#if _DEBUG
	//	CEXCDSBridge::GetInstance()->DisplayUserMessage("EXCDS Bridge [DEBUG]", "Msg Recieve", e.get_message()->get_map()["message"]->get_string().c_str(), true, true, true, true, true);
	//#endif

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

		CEXCDSBridge::SendEuroscopeMessage(response->get_map()["callsign"]->get_string().c_str(), "Cannot modify.", "ALREADY_TRACKED");
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

// From VATCANsitu
void MessageHandler::SendKeyboardPresses(std::vector<WORD> message)
{
	std::vector<INPUT> vec;
	for (auto ch : message)
	{
		INPUT input = { 0 };
		input.type = INPUT_KEYBOARD;
		input.ki.dwFlags = KEYEVENTF_SCANCODE;
		input.ki.time = 0;
		input.ki.wVk = 0;
		input.ki.wScan = ch;
		input.ki.dwExtraInfo = 1;
		vec.push_back(input);

		input.ki.dwFlags |= KEYEVENTF_KEYUP;
		vec.push_back(input);
	}

	SendInput(vec.size(), &vec[0], sizeof(INPUT));
}

// From VATCANsitu
void MessageHandler::SendKeyboardString(const std::string str)
{
	std::vector<INPUT> vec;
	const auto key_board_layout = GetKeyboardLayout(0);

	for (auto ch : str) {
		bool shift_needed = isupper(ch) || (ispunct(ch) && VkKeyScanExW(ch, key_board_layout) & 0x0100);

		if (shift_needed) {
			INPUT inputShift = { 0 };
			inputShift.type = INPUT_KEYBOARD;
			inputShift.ki.wVk = VK_SHIFT;
			inputShift.ki.dwFlags = 0;
			vec.push_back(inputShift);
		}

		INPUT input = { 0 };
		input.type = INPUT_KEYBOARD;
		input.ki.wVk = VkKeyScanExW(tolower(ch), key_board_layout) & 0xFF;
		input.ki.wScan = MapVirtualKeyExW(input.ki.wVk, MAPVK_VK_TO_VSC, key_board_layout);
		input.ki.dwFlags = 0;
		vec.push_back(input);

		input.ki.dwFlags |= KEYEVENTF_KEYUP;
		vec.push_back(input);

		if (shift_needed) {
			INPUT inputShift = { 0 };
			inputShift.type = INPUT_KEYBOARD;
			inputShift.ki.wVk = VK_SHIFT;
			inputShift.ki.dwFlags = KEYEVENTF_KEYUP;
			vec.push_back(inputShift);
		}
	}

	SendInput(vec.size(), vec.data(), sizeof(INPUT));
}