#include "sio_client.h"
#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <time.h>
#include <vector>
#include <Windows.h>
#include <unordered_set>

#include "ApiHelper.h"
#include "EuroScopePlugIn.h"
#include "CEXCDSBridge.h"

#include "MessageHandler.h"

#define ENTER 0x1c
#define NUMPAD_PLUS 0x4e
#define BRACKET_CLOSE 0x1b

using namespace sio;

namespace {
	// Moved global to anonymous namespace for better encapsulation
	typedef std::tuple<std::string, EuroScopePlugIn::CPosition> EstimatePosn;
	std::vector<EstimatePosn> estimates;

	// Cache bridge instance to avoid repeated calls
	CEXCDSBridge* GetBridgeInstance() {
		static CEXCDSBridge* instance = CEXCDSBridge::GetInstance();
		return instance;
	}
}

#pragma region Update_Methods

void MessageHandler::UpdatePositions(sio::event& e)
{
	try {
		const auto positions = e.get_message()->get_map()["positions"]->get_vector();

		estimates.clear();
		estimates.reserve(positions.size());

		for (const auto& position : positions) {
			const std::string name = position->get_map()["name"]->get_string();
			const std::string lat = position->get_map()["lat"]->get_string();
			const std::string lon = position->get_map()["lon"]->get_string();

			EuroScopePlugIn::CPosition pos;
			pos.LoadFromStrings(lon.c_str(), lat.c_str());
			estimates.emplace_back(name, pos);
		}
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Failed to update positions: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
	catch (...) {
		OutputDebugString("Failed to update positions: Unknown error");
	}
}

struct HandleData {
	unsigned long process_id;
	HWND window_handle;
};

BOOL isMainWindow(HWND handle)
{
	return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
}

BOOL CALLBACK enumWindowsCallback(HWND handle, LPARAM lParam)
{
	try {
		auto& data = *reinterpret_cast<HandleData*>(lParam); // Better cast
		unsigned long process_id = 0;
		GetWindowThreadProcessId(handle, &process_id);

		if (data.process_id != process_id || !isMainWindow(handle)) {
			return TRUE;
		}

		data.window_handle = handle;
		return FALSE;
	}
	catch (...) {
		OutputDebugString("Error in enumWindowsCallback");
		return FALSE;
	}
}

HWND findMainWindow(unsigned long process_id)
{
	try {
		HandleData data{ process_id, nullptr };
		EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));
		return data.window_handle;
	}
	catch (...) {
		OutputDebugString("Error finding main window");
		return nullptr;
	}
}

void MessageHandler::SendPDC(sio::event& e)
{
	try {
		const std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		const std::string value = e.get_message()->get_map()["value"]->get_string();

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);
		const HWND mainWindow = findMainWindow(GetCurrentProcessId());

		if (mainWindow == nullptr) {
			throw std::runtime_error("Could not find main window");
		}

		SetForegroundWindow(mainWindow);
		
		// Caps Lock check and control
		BYTE keyState[256];
		GetKeyboardState(keyState);
		bool capsLockOn = (keyState[VK_CAPITAL] & 0x01) != 0;

		// Turn off Caps Lock if it's on
		if (capsLockOn) {
			INPUT input[2] = {};
			input[0].type = INPUT_KEYBOARD;
			input[0].ki.wVk = VK_CAPITAL;
			input[0].ki.dwFlags = 0;
			input[1] = input[0];
			input[1].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(2, input, sizeof(INPUT));
		}

		std::string pdcTarget;
		pdcTarget.reserve(7 + callsign.length());
		pdcTarget = ".chat " + callsign;

		SendKeyboardString(pdcTarget);
		SendKeyboardPresses({ ENTER });
		SendKeyboardString(value);
		SendKeyboardPresses({ ENTER });
		
		// Restore Caps Lock if it was on
		if (capsLockOn) {
			INPUT input[2] = {};
			input[0].type = INPUT_KEYBOARD;
			input[0].ki.wVk = VK_CAPITAL;
			input[0].ki.dwFlags = 0;
			input[1] = input[0];
			input[1].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(2, input, sizeof(INPUT));
		}

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "PDC Error: " + std::string(ex.what());
		CEXCDSBridge::SendEuroscopeMessage("PDC WARNING", errorMsg.c_str(), "UNKNOWN");
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::SendFrequencyMessage(sio::event& e)
{
	try {
		const std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		const std::string value = e.get_message()->get_map()["value"]->get_string();

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);
		const HWND mainWindow = findMainWindow(GetCurrentProcessId());

		if (mainWindow == nullptr) {
			throw std::runtime_error("Could not find main window");
		}

		SetForegroundWindow(mainWindow);
		SendKeyboardString(callsign);
		SendKeyboardPresses({ NUMPAD_PLUS, 0x0E }); // 0x0E is backspace
		SendKeyboardString(value);
		SendKeyboardPresses({ BRACKET_CLOSE });

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Frequency Message Error: " + std::string(ex.what());
		CEXCDSBridge::SendEuroscopeMessage("PDC WARNING", errorMsg.c_str(), "UNKNOWN");
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::SendRawTextMessage(sio::event& e)
{
	try {
		const std::string value = e.get_message()->get_map()["value"]->get_string();

		auto response = object_message::create();
		const HWND mainWindow = findMainWindow(GetCurrentProcessId());

		if (mainWindow == nullptr) {
			throw std::runtime_error("Could not find main window");
		}

		SetForegroundWindow(mainWindow);
		SendKeyboardString(value);
		SendKeyboardPresses({ ENTER });

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Raw Text Message Error: " + std::string(ex.what());
		CEXCDSBridge::SendEuroscopeMessage("PDC WARNING", errorMsg.c_str(), "UNKNOWN");
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::RequestSectorData(sio::event& e)
{
	try {
		auto* bridgeInstance = GetBridgeInstance();
		auto elements = sio::array_message::create();

		auto sectorElement = bridgeInstance->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRSPACE);

		while (sectorElement.IsValid()) {
			auto msg = sio::object_message::create();

			msg->get_map()["type"] = string_message::create("Feature");
			msg->get_map()["properties"] = sio::object_message::create();
			msg->get_map()["properties"]->get_map()["name"] = string_message::create(sectorElement.GetName());

			msg->get_map()["geometry"] = sio::object_message::create();
			msg->get_map()["geometry"]->get_map()["type"] = string_message::create("LineString");
			msg->get_map()["geometry"]->get_map()["coordinates"] = sio::array_message::create();

			int i = 0;
			while (i < 100) {
				EuroScopePlugIn::CPosition* pos = nullptr;

				const bool success = sectorElement.GetPosition(pos, i);
				if (!success || pos == nullptr) {
					break;
				}

				auto coord = sio::array_message::create();
				coord->get_vector().push_back(double_message::create(pos->m_Latitude));
				coord->get_vector().push_back(double_message::create(pos->m_Longitude));

				msg->get_map()["geometry"]->get_map()["coordinates"]->get_vector().push_back(coord);
				++i;
			}

			elements->get_vector().push_back(msg);
			sectorElement = bridgeInstance->SectorFileElementSelectNext(sectorElement, EuroScopePlugIn::SECTOR_ELEMENT_AIRSPACE);
		}

		bridgeInstance->GetSocket()->emit("SEND_MAP_DATA", elements);
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Sector Data Error: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::HandoffTarget(sio::event& e)
{
	try {
		const std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		const std::string cjs = e.get_message()->get_map()["cjs"]->get_string();

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		const auto nextController = GetBridgeInstance()->ControllerSelectByPositionId(cjs.c_str());
		if (!nextController.IsValid()) {
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot handoff", "UNKNOWN");
			return;
		}

		const bool isAssigned = fp.InitiateHandoff(nextController.GetCallsign());
		if (!isAssigned) {
			e.put_ack_message(NotModified(response, "Could not handoff target."));
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "UNKNOWN");
			return;
		}

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Handoff Error: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::RefuseHandoff(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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
		std::string id = e.get_message()->get_map()["es_id"]->get_string();

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());
		EuroScopePlugIn::CRadarTarget rt = CEXCDSBridge::GetInstance()->RadarTargetSelect(id.c_str());

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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

		auto response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

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

void MessageHandler::UpdateAircraftState(sio::event& e)
{
	try {
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string etd = e.get_message()->get_map()["etd"]->get_string();
		std::string ete_hours = e.get_message()->get_map()["ete_hours"]->get_string();
		std::string ete_mins = e.get_message()->get_map()["ete_mins"]->get_string();
		std::string depart = e.get_message()->get_map()["depart"]->get_string();
		std::string dest = e.get_message()->get_map()["dest"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		if (etd.size() == 4) {
			fp.GetFlightPlanData().SetEstimatedDepartureTime(etd.c_str());
		}

		if (ete_hours.size() == 2 && ete_mins.size() == 2) {
			fp.GetFlightPlanData().SetEnrouteHours(ete_hours.c_str());
			fp.GetFlightPlanData().SetEnrouteMinutes(ete_mins.c_str());
		}

		if (depart.size() == 4) {
			fp.GetFlightPlanData().SetOrigin(depart.c_str());
		}

		if (dest.size() == 4) {
			fp.GetFlightPlanData().SetDestination(dest.c_str());
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
			// fp.GetFlightPlanData().SetFinalAltitude(final);
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

void MessageHandler::InitiateCoordination(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string point = e.get_message()->get_map()["point"]->get_string();
		int altitude = e.get_message()->get_map()["altitude"]->get_int();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!fp.IsValid()) {
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot modify.", "Flight Plan not found");
			return;
		}

		bool isAssigned = false;

		std::string controller = fp.GetCoordinatedNextController();
		if (!fp.GetTrackingControllerIsMe()) {
			controller = fp.GetTrackingControllerCallsign();
		}

		if (controller.length() > 0) {
			isAssigned = fp.InitiateCoordination(controller.c_str(), point.c_str(), altitude);
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

void MessageHandler::UpdateSpeed(sio::event& e)
{
	try {
		// Get aircraft data from EXCDS
		std::string id = e.get_message()->get_map()["id"]->get_string();
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		int assignedMach = e.get_message()->get_map()["assigned_mach"]->get_int();
		int assignedSpeed = e.get_message()->get_map()["assigned_speed"]->get_int();
		int filedSpeed = e.get_message()->get_map()["filed_speed"]->get_int();

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

		bool isAssigned = false;
		auto& controllerData = fp.GetControllerAssignedData();

		if (assignedMach > 0) {
			isAssigned = controllerData.SetAssignedMach(assignedMach);
		}
		else if (assignedSpeed > 0) {
			isAssigned = controllerData.SetAssignedSpeed(assignedSpeed);
		}
		else {
			// Clear assigned speed/mach (RESUME, MCS, BFS)
			bool clearedSpeed = controllerData.SetAssignedSpeed(0);
			bool clearedMach = controllerData.SetAssignedMach(0);
			isAssigned = clearedSpeed || clearedMach;
		}

		// Assign the filed speed, if provided
		if (filedSpeed > 0) {
			isAssigned = fp.GetFlightPlanData().SetTrueAirspeed(filedSpeed) || isAssigned;
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

void MessageHandler::PushFlightStrip(sio::event& e)
{
	try {
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string cjs = e.get_message()->get_map()["cjs"]->get_string();

		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

		EuroScopePlugIn::CFlightPlan fp = bridgeInstance->FlightPlanSelect(callsign.c_str());

		fp.PushFlightStrip(cjs.c_str());
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to push flight strip");
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

void MessageHandler::UpdateCommuncationType(sio::event& e)
{
	try {
		// Gate aircraft data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		std::string type = e.get_message()->get_map()["type"]->get_string();

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Is the flight plan valid?
		if (!FlightPlanChecks(fp, response, e)) {
			return;
		}

		char val = fp.GetControllerAssignedData().GetCommunicationType();
		if (strcmp(type.c_str(), "V") == 0) val = 'V';
		else if (strcmp(type.c_str(), "T") == 0) val = 'T';
		else if (strcmp(type.c_str(), "R") == 0) val = 'R';

		bool isAssigned = fp.GetControllerAssignedData().SetCommunicationType(val);

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

		EuroScopePlugIn::CRadarTarget rt = CEXCDSBridge::GetInstance()->RadarTargetSelect(callsign.c_str());

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
		fp.GetFlightPlanData().SetEnrouteHours("0");
		fp.GetFlightPlanData().SetEnrouteMinutes("00");
		fp.GetFlightPlanData().SetEstimatedDepartureTime("0000");
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

void MessageHandler::SyncAnnotations(sio::event& e)  
{  
    try {
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
        const auto annotations = e.get_message()->get_map()["annotations"]->get_vector();

		if (annotations.size() != 6) {
			OutputDebugString("EXCDS Error: Failed to sync annotations - annotation size disagree");
			return;
		}

        std::vector<std::string> annotationStrings;
        annotationStrings.reserve(annotations.size());

		for (const auto& annotation : annotations) {
			if (annotation->get_flag() == sio::message::flag_string)
				annotationStrings.push_back(annotation->get_string());
			else
				annotationStrings.push_back("");
		}

		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);
		if (!FlightPlanChecks(fp, response, e)) return;

		fp.GetControllerAssignedData().SetFlightStripAnnotation(1, annotationStrings[0].c_str());
		fp.GetControllerAssignedData().SetFlightStripAnnotation(2, annotationStrings[1].c_str());
		fp.GetControllerAssignedData().SetFlightStripAnnotation(3, annotationStrings[2].c_str());
		fp.GetControllerAssignedData().SetFlightStripAnnotation(4, annotationStrings[3].c_str());
		fp.GetControllerAssignedData().SetFlightStripAnnotation(5, annotationStrings[4].c_str());
		fp.GetControllerAssignedData().SetFlightStripAnnotation(7, annotationStrings[5].c_str());

		const auto ctrls = e.get_message()->get_map()["controllers"]->get_vector();

		if (ctrls.size() == 0) {
			OutputDebugString("Aborting annotation sync, no controllers provided");
			return;
		}

		std::vector<std::string> controllers;
		controllers.reserve(controllers.size());

		for (const auto& c : ctrls) {
			if (c->get_flag() == sio::message::flag_string)
				controllers.push_back(c->get_string());
			else
				controllers.push_back("");
		}

		for (const auto& controller : controllers) {
			EuroScopePlugIn::CController ctrlr = CEXCDSBridge::GetInstance()->ControllerSelectByPositionId(controller.c_str());
			if (ctrlr.IsValid()) {
				fp.PushFlightStrip(ctrlr.GetPositionId());
			}
			else {
				OutputDebugString(("Failed to push strip to controller " + controller + ", invalid controller").c_str());
			}
		}

		response->get_map()["modified"] = bool_message::create(true);
		e.put_ack_message(response);
    } catch (const std::exception& ex) {  
        const std::string errorMsg = "Failed to sync annotations: " + std::string(ex.what());  
        OutputDebugString(errorMsg.c_str());  
    } catch (...) {  
        OutputDebugString("Failed to sync annotations: Unknown error");  
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
	try {

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
	catch (...) {}
}

void MessageHandler::RequestAllAircraft(sio::event& e)
{
	try {
		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
		EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

		sio::message::ptr arrayMessage = sio::array_message::create();

		while (flightPlan.IsValid()) {
			if (flightPlan.GetState() == 0) {
				flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
			}

			sio::message::ptr response = sio::object_message::create();
			MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response, false);

			arrayMessage->get_vector().push_back(response);

			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
		}

		bridgeInstance->GetSocket()->emit("MASS_SEND_FP_DATA", arrayMessage);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to request all aircraft");
	}
}

void MessageHandler::RequestAircraftByCallsign(sio::event& e)
{
	try {
		// Parse data from EXCDS
		std::string callsign = e.get_message()->get_map()["callsign"]->get_string();
		EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelect(callsign.c_str());

		OutputDebugString(callsign.c_str());
		OutputDebugString("-test\n");

		// Init Response
		message::ptr response = object_message::create();
		response->get_map()["callsign"] = string_message::create(callsign);

		if (!fp.IsValid())
		{
			CEXCDSBridge::SendEuroscopeMessage(callsign.c_str(), "Cannot find aircraft.", "AC_NT_FND");
			return;
		}

		MessageHandler::PrepareFlightPlanDataResponse(fp, response, true);
		CEXCDSBridge::GetSocket()->emit("SEND_FP_DATA", response);
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed to request a/c by callsign");
	}
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

				if (remarks.find("CANMANDATE") != std::string::npos || fp.GetFlightPlanData().GetAircraftWtc() != 'L')
					ADSB = false;

				char capabilites = fp.GetFlightPlanData().GetCapibilities();
				// Intentional fall through
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
		int plannedAlt = 0;
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
		std::string corrCallsign = "";
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

			corrCallsign = fp.GetCallsign();

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

			finalAltitude = fp.GetControllerAssignedData().GetFinalAltitude() / 100;
			plannedAlt = fp.GetFlightPlanData().GetFinalAltitude() / 100;

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
		response->get_map()["internal"]->get_map()["corr_callsign"] = string_message::create(corrCallsign);
		response->get_map()["internal"]->get_map()["assignedSquawk"] = string_message::create(assignedSquawk);
		response->get_map()["internal"]->get_map()["eta"] = int_message::create(eta);
		response->get_map()["internal"]->get_map()["etd"] = string_message::create(etd);
		response->get_map()["internal"]->get_map()["atd"] = string_message::create(atd);
		response->get_map()["internal"]->get_map()["sector_entry_time"] = int_message::create(sectorEntryTime);
		response->get_map()["internal"]->get_map()["sector_exit_time"] = int_message::create(sectorExitTime);
		response->get_map()["internal"]->get_map()["frequency"] = double_message::create(frequency);

		response->get_map()["annotations"] = object_message::create();
		response->get_map()["annotations"]->get_map()["0"] = string_message::create(ann1);
		response->get_map()["annotations"]->get_map()["1"] = string_message::create(ann2);
		response->get_map()["annotations"]->get_map()["2"] = string_message::create(ann3);
		response->get_map()["annotations"]->get_map()["3"] = string_message::create(ann4);
		response->get_map()["annotations"]->get_map()["4"] = string_message::create(ann5);
		response->get_map()["annotations"]->get_map()["5"] = string_message::create(ann6);
		response->get_map()["annotations"]->get_map()["6"] = string_message::create(ann7);
		response->get_map()["annotations"]->get_map()["7"] = string_message::create(ann8);
		response->get_map()["annotations"]->get_map()["8"] = string_message::create(ann9);

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
		response->get_map()["general"]->get_map()["route"] = string_message::create(ApiHelper::ToASCII(route));
		response->get_map()["general"]->get_map()["remarks"] = string_message::create(remarks);

		response->get_map()["altitude"] = object_message::create();
		response->get_map()["altitude"]->get_map()["reported"] = string_message::create(reportedAltitude);
		response->get_map()["altitude"]->get_map()["cleared"] = string_message::create(clearedAltitude);
		response->get_map()["altitude"]->get_map()["filed"] = int_message::create(finalAltitude);
		response->get_map()["altitude"]->get_map()["planned"] = int_message::create(plannedAlt);

		response->get_map()["speed"] = object_message::create();
		response->get_map()["speed"]->get_map()["estimated_mach"] = int_message::create(estimatedMach);
		response->get_map()["speed"]->get_map()["estimated_speed"] = int_message::create(estimatedIas);
		response->get_map()["speed"]->get_map()["filed"] = int_message::create(estimatedIas);
		response->get_map()["speed"]->get_map()["assigned"] = string_message::create(assignedSpeed);

		response->get_map()["points"] = pointsMessage;

		response->get_map()["id"] = string_message::create(rt.GetSystemID());
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Radar Target Response Error: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
	catch (...) {
		OutputDebugString("EXCDS Error: Failed radar target update");
	}
}

void MessageHandler::PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, message::ptr response, boolean full)
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

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	try {
		// Callsign
		response->get_map()["callsign"] = string_message::create(fp.GetCallsign());

		bool isConnected = bridgeInstance->RadarTargetSelect(fp.GetCallsign()).IsValid();
		response->get_map()["connected"] = bool_message::create(isConnected);

			fp.AcceptCoordination();
		
		// Aircraft
		std::string acType = fp.GetFlightPlanData().GetAircraftFPType();
		char wakeTurbulenceCat = fp.GetFlightPlanData().GetAircraftWtc();
		char equip = fp.GetFlightPlanData().GetCapibilities();
		std::string typeString = "/" + acType + "/";

		std::string equipment = fp.GetFlightPlanData().GetAircraftFPType();

		switch (equip) {
		case 'Q': case 'W': case 'L':
			typeString += "W";
			equipment = "W";
			break;
		case 'R':
			typeString += "R";
			equipment = "R";
			break;
		case 'G': case 'Y': case 'C': case 'I':
			typeString += "G";
			equipment = "G";
			break;
		case 'E':case 'F':
			typeString += "E";
			equipment = "E";
			break;
		case 'A': case 'T':
			typeString += "S";
			equipment = "S";
			break;
		default:
			typeString += "N";
			equipment = "N";
		}

		typeString.insert(0, 1, wakeTurbulenceCat);

		//int equipmentIndex = equipment.find_last_of('/');
		//if (equipmentIndex > 0)
		//	equipment = equipment.substr(equipment.find_last_of('/'));
		//else
		//	equipment = equip;

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
		case 'P':
		case 'E':
			engine = "P";
		default:
			engine = "J";
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
		response->get_map()["annotations"]->get_map()["0"] = string_message::create(ann1);
		response->get_map()["annotations"]->get_map()["1"] = string_message::create(ann2);
		response->get_map()["annotations"]->get_map()["2"] = string_message::create(ann3);
		response->get_map()["annotations"]->get_map()["3"] = string_message::create(ann4);
		response->get_map()["annotations"]->get_map()["4"] = string_message::create(ann5);
		response->get_map()["annotations"]->get_map()["5"] = string_message::create(ann6);
		response->get_map()["annotations"]->get_map()["6"] = string_message::create(ann7);
		response->get_map()["annotations"]->get_map()["7"] = string_message::create(ann8);
		response->get_map()["annotations"]->get_map()["8"] = string_message::create(ann9);

		// Altitude Information
		int final = fp.GetControllerAssignedData().GetFinalAltitude();
		if (final == 0)
			final = fp.GetFlightPlanData().GetFinalAltitude();

		int planned = fp.GetFlightPlanData().GetFinalAltitude();

		std::string filedAltString;
		if (final == 0)
			filedAltString = "fld";
		else if (final < 18000)
			filedAltString = "A" + std::to_string(final / 100);
		else
			filedAltString = "F" + std::to_string(final / 100);

		int clearedAlt = fp.GetControllerAssignedData().GetClearedAltitude();
		std::string clearedAltString;
		if (clearedAlt == 0) {
			clearedAlt = final;
			clearedAltString = std::to_string(final / 100); // Altitude not assigned, assume it is equal to cruise
		}
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
		response->get_map()["altitude"]->get_map()["final"] = object_message::create();
		response->get_map()["altitude"]->get_map()["final"]->get_map()["abbr"] = string_message::create(filedAltString);
		response->get_map()["altitude"]->get_map()["final"]->get_map()["value"] = int_message::create(final);
		response->get_map()["altitude"]->get_map()["planned"] = int_message::create(planned);

		if (
			fp.GetCorrelatedRadarTarget().IsValid() &&
			fp.GetCorrelatedRadarTarget().GetPosition().IsValid() &&
			fp.GetCorrelatedRadarTarget().GetPosition().GetRadarFlags() > 1 &&
			fp.GetState() > 0
		) {
			EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();

			int modec = 0;
			if (rt.GetPosition().GetFlightLevel() >= 18000)
				modec = (rt.GetPosition().GetFlightLevel() + 50) / 100;
			else
				modec = (rt.GetPosition().GetPressureAltitude() + 50) / 100;

			int vs = rt.GetVerticalSpeed();

			response->get_map()["altitude"]->get_map()["mode_c"] = int_message::create(modec);
			response->get_map()["altitude"]->get_map()["vsr"] = int_message::create(vs);

			bool altError = false;
			if ((modec > (clearedAlt / 100) + 2 || modec < (clearedAlt / 100) - 2)) {
				altError = true;
			}

			response->get_map()["altitude"]->get_map()["alt_error"] = bool_message::create(altError);

			try {
			response->get_map()["radar"] = object_message::create();
			response->get_map()["radar"]->get_map()["lat"] = double_message::create(rt.GetPosition().GetPosition().m_Latitude);
			response->get_map()["radar"]->get_map()["lon"] = double_message::create(rt.GetPosition().GetPosition().m_Longitude);
			} catch (...) {}
		}

		// Coordination
		response->get_map()["coordination"] = object_message::create();
		response->get_map()["coordination"]->get_map()["enter_alt_value"] = int_message::create(fp.GetEntryCoordinationAltitude());
		response->get_map()["coordination"]->get_map()["enter_alt_state"] = int_message::create(fp.GetEntryCoordinationAltitudeState());
		response->get_map()["coordination"]->get_map()["enter_point"] = string_message::create(fp.GetEntryCoordinationPointName());
		response->get_map()["coordination"]->get_map()["enter_point_state"] = int_message::create(fp.GetEntryCoordinationPointState());
		response->get_map()["coordination"]->get_map()["exit_alt_value"] = int_message::create(fp.GetExitCoordinationAltitude());
		response->get_map()["coordination"]->get_map()["exit_alt_state"] = int_message::create(fp.GetExitCoordinationAltitudeState());
		response->get_map()["coordination"]->get_map()["exit_point"] = string_message::create(fp.GetExitCoordinationPointName());
		response->get_map()["coordination"]->get_map()["exit_point_state"] = int_message::create(fp.GetExitCoordinationNameState());
		response->get_map()["coordination"]->get_map()["next_controller_state"] = int_message::create(fp.GetCoordinatedNextControllerState());

		// Controller Assigned Data
		std::string excdsGroundStatus = "";

		const char* groundStatus = fp.GetGroundState();
		bool cleared = fp.GetClearenceFlag();
		const char* scratchPad = fp.GetControllerAssignedData().GetScratchPadString();
		if (strcmp(scratchPad, "RREL ") == 0 || strcmp(scratchPad, "RREL") == 0)
			excdsGroundStatus = "TXRL";
		else if (strcmp(scratchPad, "RREQ ") == 0 || strcmp(scratchPad, "RREQ") == 0)
			excdsGroundStatus = "TXRQ";
		else if (strcmp(groundStatus, "ARR") == 0)
			excdsGroundStatus = "ARR";
		else if (strcmp(groundStatus, "TXIN") == 0)
			excdsGroundStatus = "TXIN";
		else if (strcmp(groundStatus, "PARK") == 0)
			excdsGroundStatus = "PARK";
		else if (strcmp(groundStatus, "TAXI") == 0)
			excdsGroundStatus = "TAXI";
		else if (strcmp(groundStatus, "PUSH") == 0)
			excdsGroundStatus = "PUSH";
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

		response->get_map()["controllerData"] = object_message::create();
		response->get_map()["controllerData"]->get_map()["heading"] = int_message::create(fp.GetControllerAssignedData().GetAssignedHeading());
		response->get_map()["controllerData"]->get_map()["ground_status"] = string_message::create(excdsGroundStatus);
		response->get_map()["controllerData"]->get_map()["scratchpad"] = string_message::create(fp.GetControllerAssignedData().GetScratchPadString());
		response->get_map()["controllerData"]->get_map()["special_status"] = string_message::create(excdsStatus);
		response->get_map()["controllerData"]->get_map()["fp_tracking_state"] = int_message::create(fp.GetFPState());
		response->get_map()["controllerData"]->get_map()["squawk"] = string_message::create(fp.GetControllerAssignedData().GetSquawk());
		response->get_map()["controllerData"]->get_map()["controller_tracking_state"] = int_message::create(fpstate);

		try {
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

		// Route Information
		std::string dep = fp.GetFlightPlanData().GetOrigin();
		std::string depRwy = fp.GetFlightPlanData().GetDepartureRwy();
		std::string sid = fp.GetFlightPlanData().GetSidName();

		std::string dest = fp.GetFlightPlanData().GetDestination();
		std::string arrRwy = fp.GetFlightPlanData().GetArrivalRwy();
		std::string star = fp.GetFlightPlanData().GetStarName();

		std::string route = fp.GetFlightPlanData().GetRoute();

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
		response->get_map()["route"]->get_map()["text"] = string_message::create(ApiHelper::ToASCII(route));

		if (fp.GetState() > 0 || full) {
			EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
			EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

			boolean eastbound = true;
			if (origin.DirectionTo(destination) > 179)
				eastbound = false;

			std::string first = fp.GetExtractedRoute().GetPointName(1);

			int sectorEntryTime = fp.GetSectorEntryMinutes();
			int sectorExitTime = fp.GetSectorExitMinutes();

			EuroScopePlugIn::CPosition sectorExit = fp.GetPositionPredictions().GetPosition(sectorEntryTime);
			double sectorExitLat = sectorExit.m_Latitude;
			double sectorExitLong = sectorExit.m_Longitude;
			response->get_map()["route"]->get_map()["eastbound"] = bool_message::create(eastbound);
			response->get_map()["route"]->get_map()["first_fix"] = string_message::create(ApiHelper::ToASCII(first));
			response->get_map()["route"]->get_map()["track"] = int_message::create(origin.DirectionTo(destination));
			response->get_map()["route"]->get_map()["sector_exit_lat"] = double_message::create(sectorExitLat);
			response->get_map()["route"]->get_map()["sector_exit_lon"] = double_message::create(sectorExitLong);
			response->get_map()["route"]->get_map()["sector_entry_time"] = int_message::create(sectorEntryTime);
			response->get_map()["route"]->get_map()["sector_exit_time"] = int_message::create(sectorExitTime);
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

		if (fp.GetCorrelatedRadarTarget().IsValid()) {
			response->get_map()["target_id"] = string_message::create(fp.GetCorrelatedRadarTarget().GetSystemID());
		}

		response->get_map()["estimates"] = object_message::create();

		// EXCDs estimate
		std::string arrivalEstimateName = "";
		int arrivalEstimateTime = -1;

		arrivalEstimateName = fp.GetFlightPlanData().GetDestination();
		arrivalEstimateTime = fp.GetPositionPredictions().GetPointsNumber() - 1;

		response->get_map()["estimates"]->get_map()["arrival_time"] = int_message::create(arrivalEstimateTime);
		response->get_map()["estimates"]->get_map()["arrival_fix"] = string_message::create(arrivalEstimateName);

		if (fp.GetState() > 1 || full)
		{
			EuroScopePlugIn::CPosition origin = fp.GetExtractedRoute().GetPointPosition(0);
			EuroScopePlugIn::CPosition destination = fp.GetExtractedRoute().GetPointPosition(fp.GetExtractedRoute().GetPointsNumber() - 1);

			sio::message::ptr pointsMessage = sio::array_message::create();

			for (int i = 0; i < fp.GetExtractedRoute().GetPointsNumber(); i++) {
				sio::message::ptr msg = sio::object_message::create();

				EuroScopePlugIn::CPosition pos = fp.GetExtractedRoute().GetPointPosition(i);
				msg->get_map()["lat"] = double_message::create(pos.m_Latitude);
				msg->get_map()["long"] = double_message::create(pos.m_Longitude);
				msg->get_map()["eta"] = double_message::create(fp.GetExtractedRoute().GetPointDistanceInMinutes(i));

				msg->get_map()["name"] = string_message::create(fp.GetExtractedRoute().GetPointName(i));

				pointsMessage->get_vector().push_back(msg);
			}

			response->get_map()["points"] = pointsMessage;
		}
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

	try {
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
	catch (...)
	{
		return false;
	}
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
	try {
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
	catch (...) {}

}

bool MessageHandler::StatusAssign(std::string status, EuroScopePlugIn::CFlightPlan fp, std::string departureTime = "")
{
	try {
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
			success = fp.GetControllerAssignedData().SetScratchPadString("RREQ ");
		}
		else if (strcmp(status.c_str(), "TXRL") == 0)
		{
			success = fp.GetControllerAssignedData().SetScratchPadString("TAXI");
			success = fp.GetControllerAssignedData().SetScratchPadString("CLEA");

			OutputDebugString("TXRL");

			// 1 = FSS / 5 = APP/DEP / 6 = CTR
			if (CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() > 1 && CEXCDSBridge::GetInstance()->ControllerMyself().GetFacility() < 5)
				return false;

			success = fp.GetControllerAssignedData().SetScratchPadString("RREL ");
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
	catch (...) {
		return false;
	}
}

int nextAvail = 1;
std::string MessageHandler::SquawkGenerator(std::string squawkPrefix)
{
	try {
		std::unordered_set<std::string> assignedSquawks = {
			"1201", "1202", "1234", "1255", "1277", "4453"
		};

		// First, collect all assigned squawk codes
		for (
			EuroScopePlugIn::CFlightPlan fp = CEXCDSBridge::GetInstance()->FlightPlanSelectFirst();
			fp.IsValid();
			fp = CEXCDSBridge::GetInstance()->FlightPlanSelectNext(fp)
			) {
			assignedSquawks.insert(fp.GetControllerAssignedData().GetSquawk());
		}

		bool wasReset = false;
		int start = nextAvail;
		if (start > 78) start = 1; // Reset to 001 if we exceed 7777

		// Now, generate and check for an available squawk code
		for (int i = nextAvail; i <= 78; i++) {
			if (i == 78) {
				if (wasReset) {
					return "";
				}

				i = 0;
				wasReset = true;
				continue;
			}
			
			if (i % 10 > 7) continue;

			std::string squawkSuffix = std::to_string(i);
			squawkSuffix.insert(squawkSuffix.begin(), 2 - squawkSuffix.length(), '0'); // Pad to 2 digits
			std::string transponder = squawkPrefix + squawkSuffix;

			if (assignedSquawks.find(transponder) == assignedSquawks.end()) {
				nextAvail = i + 1;
				return transponder;
			}
		}

		return "";
	}
	catch (...) {
		return "";
	}
}

// From VATCANsitu
void MessageHandler::SendKeyboardPresses(const std::vector<WORD>& message)
{
	if (message.empty()) {
		return;
	}

	try {
		std::vector<INPUT> vec;
		vec.reserve(message.size() * 2); // Performance: Reserve capacity

		for (const auto ch : message) {
			INPUT input{};
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

		const UINT result = SendInput(static_cast<UINT>(vec.size()), vec.data(), sizeof(INPUT));
		if (result != vec.size()) {
			OutputDebugString("Warning: Not all keyboard inputs were sent successfully");
		}
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Keyboard Press Error: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
}

void MessageHandler::SendKeyboardString(const std::string& str)
{
	if (str.empty()) {
		return;
	}

	try {
		std::vector<INPUT> vec;
		vec.reserve(str.length() * 4); // Performance: Reserve capacity (worst case with shift)

		const auto keyboardLayout = GetKeyboardLayout(0);

		for (const auto ch : str) {
			const bool shiftNeeded = isupper(ch) || (ispunct(ch) && VkKeyScanExW(ch, keyboardLayout) & 0x0100);

			if (shiftNeeded) {
				INPUT inputShift{};
				inputShift.type = INPUT_KEYBOARD;
				inputShift.ki.wVk = VK_SHIFT;
				inputShift.ki.dwFlags = 0;
				vec.push_back(inputShift);
			}

			INPUT input{};
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = VkKeyScanExW(tolower(ch), keyboardLayout) & 0xFF;
			input.ki.wScan = MapVirtualKeyExW(input.ki.wVk, MAPVK_VK_TO_VSC, keyboardLayout);
			input.ki.dwFlags = 0;
			vec.push_back(input);

			input.ki.dwFlags |= KEYEVENTF_KEYUP;
			vec.push_back(input);

			if (shiftNeeded) {
				INPUT inputShift{};
				inputShift.type = INPUT_KEYBOARD;
				inputShift.ki.wVk = VK_SHIFT;
				inputShift.ki.dwFlags = KEYEVENTF_KEYUP;
				vec.push_back(inputShift);
			}
		}

		const UINT result = SendInput(static_cast<UINT>(vec.size()), vec.data(), sizeof(INPUT));
		if (result != vec.size()) {
			OutputDebugString("Warning: Not all keyboard string inputs were sent successfully");
		}
	}
	catch (const std::exception& ex) {
		const std::string errorMsg = "Keyboard String Error: " + std::string(ex.what());
		OutputDebugString(errorMsg.c_str());
	}
}