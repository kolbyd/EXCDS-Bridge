#include "pch.h"
#include "sio_client.h"

#include <afxsock.h>
#include <iostream>
#include "stdio.h"
#include "MessageHandler.h"
#include "CEXCDSBridge.h"

#define PLUGIN_NAME		"EXCDS Bridge"
#define PLUGIN_VERSION	"0.0.5-alpha"
#define PLUGIN_AUTHOR	"Kolby Dunning / Liam Shaw"
#define PLUGIN_LICENSE	"Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"

const std::string BRIDGE_HOST = "http://127.0.0.1";
const std::string BRIDGE_PORT = "7500";

// The socket connection between the EXCDS program and EuroScope
sio::client socketClient;

// The instance of the bridge, for accessing EuroScope functions from other classes
CEXCDSBridge* instance;

CEXCDSBridge::CEXCDSBridge() :
	EuroScopePlugIn::CPlugIn(
		EuroScopePlugIn::COMPATIBILITY_CODE,
		PLUGIN_NAME,
		PLUGIN_VERSION,
		PLUGIN_AUTHOR,
		PLUGIN_LICENSE
	)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Set instance
	instance = this;

	socketClient.connect(BRIDGE_HOST + ":" + BRIDGE_PORT);
	socketClient.socket()->emit("CONNECTED", sio::message::list("true"));

	// Register for the socket events
	bind_events();
}

CEXCDSBridge::~CEXCDSBridge()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Cleanup socket
	socketClient.socket()->emit("CONNECTED", sio::message::list("false"));
	socketClient.socket()->off_all();
	socketClient.close();
}

void CEXCDSBridge::bind_events()
{
	MessageHandler messageHandler;

	// Messages FROM EXCDS, to update aircraft in EuroScope
	socketClient.socket()->on("UPDATE_ALTITUDE", std::bind(&MessageHandler::UpdateAltitude, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SPEED", std::bind(&MessageHandler::UpdateSpeed, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FLIGHTPLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_STATUS", std::bind(&MessageHandler::UpdateAircraftStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DEPARTURE_INSTRUCTIONS", std::bind(&MessageHandler::UpdateDepartureInstructions, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ARRIVAL_INSTRUCTIONS", std::bind(&MessageHandler::UpdateArrivalInstructions, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_TRACKING_STATUS", std::bind(&MessageHandler::UpdateTrackingStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ESTIMATE", std::bind(&MessageHandler::UpdateEstimate, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DIRECT", std::bind(&MessageHandler::UpdateDirectTo, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FLIGHT_PLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));

	// Interacts with SITU
	socketClient.socket()->on("REQUEST_RELEASE", std::bind(&MessageHandler::UpdateSitu, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("GRANT_RELEASE", std::bind(&MessageHandler::UpdateSitu, &messageHandler, std::placeholders::_1));
	//socketClient.socket()->on("TRIGGER_MISSED_APPROACH", std::bind(&MessageHandler::UpdateMissedApproach, &messageHandler, std::placeholders::_1));

	// EXCDS information requests
	socketClient.socket()->on("REQUEST_ALL_FP_DATA", std::bind(&MessageHandler::RequestAllAircraft, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_FP_DATA_CALLSIGN", std::bind(&MessageHandler::RequestAircraftByCallsign, &messageHandler, std::placeholders::_1));
}

std::vector<std::string> departureAirports = {};
std::vector<std::string> arrivalAirports = {};

std::vector<std::tuple<std::string, std::string, std::string>> UnencodedEXCDSEstimatePositions;
std::vector<std::tuple<EuroScopePlugIn::CPosition, std::string>> EXCDSEstimatePositions;

void CEXCDSBridge::OnAirportRunwayActivityChanged()
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	for (EuroScopePlugIn::CSectorElement airport = bridgeInstance->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT); airport.IsValid(); airport = bridgeInstance->SectorFileElementSelectNext(airport, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT))
	{
		if (airport.IsElementActive(true))
			departureAirports.push_back(airport.GetName());

		if (airport.IsElementActive(false))
			arrivalAirports.push_back(airport.GetName());
	}

}

void CEXCDSBridge::OnTimer(int Counter)
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	if (departureAirports.size() == 0) {
		for (EuroScopePlugIn::CSectorElement airport = bridgeInstance->SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT); airport.IsValid(); airport = bridgeInstance->SectorFileElementSelectNext(airport, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT))
		{
			if (airport.IsElementActive(true))
				departureAirports.push_back(airport.GetName());

			if (airport.IsElementActive(false))
				arrivalAirports.push_back(airport.GetName());
		}
	}

	if (Counter < 1)
	{
		UnencodedEXCDSEstimatePositions.emplace_back("");

		for (int i = 0; i < UnencodedEXCDSEstimatePositions.size(); i++)
		{
			EuroScopePlugIn::CPosition pos;
			EXCDSEstimatePositions.emplace_back(pos.LoadFromStrings(UnencodedEXCDSEstimatePositions[i][0], UnencodedEXCDSEstimatePositions[i][1]));

		}
	}

	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();
	EuroScopePlugIn::CRadarTarget radarTarget = bridgeInstance->RadarTargetSelectFirst();

	sio::message::ptr lists = sio::object_message::create();
	lists->get_map()["vfr_collector"] = sio::object_message::create();
	lists->get_map()["vfr_control"] = sio::object_message::create();
	lists->get_map()["departures"] = sio::object_message::create();
	lists->get_map()["arrivals"] = sio::object_message::create();
	lists->get_map()["apt_arrivals"] = sio::object_message::create();

	while (flightPlan.IsValid()) {
		if (flightPlan.GetState() == 0) {
			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
			continue;
		};

		// Generic
		std::string callsign = flightPlan.GetCallsign();

		std::string wtcat = " ";
		switch (flightPlan.GetFlightPlanData().GetAircraftWtc()) {
		case 'L':
			wtcat = "-";
			break;
		case 'H':
			wtcat = "+";
			break;
		case 'J':
			wtcat = "J";
			break;
		}

		std::string scratchpad = flightPlan.GetControllerAssignedData().GetScratchPadString();
		std::string sfi = scratchpad.length() == 1 ? scratchpad : "";

		std::string cjs = flightPlan.GetTrackingControllerId();

		std::string type = flightPlan.GetFlightPlanData().GetAircraftFPType();

		std::string transponder = flightPlan.GetControllerAssignedData().GetSquawk();

		int clearedAltitude = flightPlan.GetControllerAssignedData().GetClearedAltitude() == 0 ? flightPlan.GetFlightPlanData().GetFinalAltitude() : flightPlan.GetControllerAssignedData().GetClearedAltitude();

		std::string remarks = flightPlan.GetFlightPlanData().GetRemarks();
		bool medevac = false;
		if (remarks.find("STS/MEDEVAC") > 0)
			medevac = true;

		// ModeC
		int modeC = -1;
		std::string vmi = "";
		std::string currentAltitude = "";
		if (flightPlan.GetCorrelatedRadarTarget().IsValid())
		{
			if (flightPlan.GetCorrelatedRadarTarget().GetPosition().GetFlightLevel() >= 18000)
				modeC = (flightPlan.GetCorrelatedRadarTarget().GetPosition().GetFlightLevel() + 50) / 100;
			else
				modeC = (flightPlan.GetCorrelatedRadarTarget().GetPosition().GetPressureAltitude() + 50) / 100;

			if (flightPlan.GetCorrelatedRadarTarget().GetVerticalSpeed() > 200)
				vmi = "^";
			else if (flightPlan.GetCorrelatedRadarTarget().GetVerticalSpeed() < -200)
				vmi = "|";
			else if (modeC - 200 > clearedAltitude)
				vmi = "\\";
			else if (modeC + 200 < clearedAltitude)
				vmi = "/";
		}
		if (clearedAltitude == 1 || clearedAltitude == 2)
			currentAltitude = "APR";
		else
			currentAltitude = std::to_string(clearedAltitude / 100);

		// Arrival data
		std::string ades = flightPlan.GetFlightPlanData().GetDestination();
		ades = ades.length() == 4 ? ades : "ZZZZ";
		int eta = flightPlan.GetPositionPredictions().GetPointsNumber();
		std::string arrivalRunway = flightPlan.GetFlightPlanData().GetArrivalRwy();

		// Departure data
		std::string adep = flightPlan.GetFlightPlanData().GetOrigin();
		adep = adep.length() == 4 ? adep : "ZZZz";
		std::string etd = flightPlan.GetFlightPlanData().GetEstimatedDepartureTime();
		std::string finalAlt = "fld";
		if (flightPlan.GetFlightPlanData().GetFinalAltitude())
			finalAlt = std::to_string(flightPlan.GetFinalAltitude() / 100);

		if (strcmp(flightPlan.GetFlightPlanData().GetPlanType(), "V") == 0)
		{
			if (flightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_ASSUMED)
			{
				lists->get_map()["vfr_control"]->get_map()[callsign] = sio::object_message::create();
				lists->get_map()["vfr_control"]->get_map()[callsign]->get_map()["0"] = sio::string_message::create(callsign);
				lists->get_map()["vfr_control"]->get_map()[callsign]->get_map()["1"] = sio::string_message::create(wtcat);
				lists->get_map()["vfr_control"]->get_map()[callsign]->get_map()["2"] = sio::string_message::create(sfi);
				lists->get_map()["vfr_control"]->get_map()[callsign]->get_map()["3"] = sio::string_message::create(ades);
			}
			else if (flightPlan.GetState() > 1)
			{
				lists->get_map()["vfr_collector"]->get_map()[callsign] = sio::object_message::create();
				lists->get_map()["vfr_collector"]->get_map()[callsign]->get_map()["0"] = sio::string_message::create(callsign);
				lists->get_map()["vfr_collector"]->get_map()[callsign]->get_map()["1"] = sio::string_message::create(wtcat);
				lists->get_map()["vfr_collector"]->get_map()[callsign]->get_map()["2"] = sio::string_message::create(sfi);
				lists->get_map()["vfr_collector"]->get_map()[callsign]->get_map()["3"] = sio::string_message::create(ades);
			}
		}
		else if (flightPlan.GetState() > EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED)
		{
			if (flightPlan.GetFPState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_NOT_STARTED &&
				std::find(departureAirports.begin(), departureAirports.end(), flightPlan.GetFlightPlanData().GetOrigin()) != departureAirports.end())
			{
				lists->get_map()["departures"]->get_map()[callsign] = sio::object_message::create();
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["0"] = sio::bool_message::create(medevac);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["1"] = sio::string_message::create(cjs);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["2"] = sio::string_message::create(callsign);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["3"] = sio::string_message::create(wtcat);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["4"] = sio::string_message::create(sfi);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["5"] = sio::string_message::create(transponder);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["6"] = sio::string_message::create(type);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["7"] = sio::string_message::create(adep);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["8"] = sio::string_message::create(finalAlt);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["9"] = sio::string_message::create(ades);
				lists->get_map()["departures"]->get_map()[callsign]->get_map()["10"] = sio::string_message::create(etd);
			}
			if ((flightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_ASSUMED || flightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_COORDINATED || flightPlan.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_TRANSFER_TO_ME_INITIATED) &&
				(std::find(arrivalAirports.begin(), arrivalAirports.end(), flightPlan.GetFlightPlanData().GetDestination()) != arrivalAirports.end()))
			{
				lists->get_map()["arrivals"]->get_map()[callsign] = sio::object_message::create();
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["0"] = sio::bool_message::create(medevac);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["1"] = sio::string_message::create(cjs);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["2"] = sio::string_message::create(callsign);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["3"] = sio::string_message::create(wtcat);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["4"] = sio::string_message::create(sfi);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["5"] = sio::string_message::create(transponder);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["6"] = sio::string_message::create(type);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["7"] = sio::int_message::create(modeC);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["8"] = sio::string_message::create(vmi);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["9"] = sio::string_message::create(currentAltitude);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["10"] = sio::string_message::create(ades);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["11"] = sio::int_message::create(eta);
				lists->get_map()["arrivals"]->get_map()[callsign]->get_map()["12"] = sio::string_message::create(arrivalRunway);
			}
		}

		sio::message::ptr response = sio::object_message::create();
		MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response);

		bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);

		flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
	}

	bridgeInstance->GetSocket()->emit("SEND_LIST_DATA", lists);

	while (radarTarget.IsValid()) {
		sio::message::ptr response = sio::object_message::create();
		MessageHandler::PrepareTargetResponse(radarTarget, response);

		bridgeInstance->GetSocket()->emit("SEND_RT_DATA", response);

		radarTarget = bridgeInstance->RadarTargetSelectNext(radarTarget);
	}

	sio::message::ptr response = sio::object_message::create();

	EuroScopePlugIn::CController controller = bridgeInstance->ControllerSelectFirst();
	while (controller.IsValid())
	{
		std::string controllerId = controller.GetPositionId();
		std::string controllerCallsign = controller.GetCallsign();
		double controllerFrequency = controller.GetPrimaryFrequency();

		response->get_map()[controllerId] = sio::object_message::create();
		response->get_map()[controllerId]->get_map()["callsign"] = sio::string_message::create(controllerCallsign);
		response->get_map()[controllerId]->get_map()["frequency"] = sio::double_message::create(controllerFrequency);

		controller = bridgeInstance->ControllerSelectNext(controller);
	}

	bridgeInstance->GetSocket()->emit("SEND_CTRLR_DATA", response);
	bridgeInstance->GetSocket()->emit("Complete");
}

void CEXCDSBridge::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	GetSocket()->emit("CALLSIGN_DISCONNECT", sio::string_message::create(FlightPlan.GetCallsign()));
}

/**
* Helper methods
*/
void CEXCDSBridge::SendEuroscopeMessage(const char* callsign, char* message, char* id)
{
	char s[256];
	sprintf_s(s, "%s (ID: %s)", message, id);

	GetInstance()->DisplayUserMessage(
		"EXCDS Bridge",
		callsign,
		s,
		true,
		true,
		false,
		false,
		false
	);
}

CEXCDSBridge* CEXCDSBridge::GetInstance()
{
	return instance;
}

sio::socket::ptr CEXCDSBridge::GetSocket()
{
	return socketClient.socket();
}
