#include "pch.h"
#include "sio_client.h"

#include <afxsock.h>
#include <iostream>
#include "stdio.h"
#include "MessageHandler.h"
#include "CEXCDSBridge.h"

#define PLUGIN_NAME		"EXCDS Bridge"
#define PLUGIN_VERSION	"0.0.4-alpha"
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
	socketClient.socket()->on("UPDATE_SCRATCHPAD", std::bind(&MessageHandler::UpdateScratchPad, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_STRIP_ANNOTATION", std::bind(&MessageHandler::UpdateStripAnnotation, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DEPARTURE_RUNWAY", std::bind(&MessageHandler::UpdateRunway, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ARRIVAL_RUNWAY", std::bind(&MessageHandler::UpdateRunway, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ALTITUDE", std::bind(&MessageHandler::UpdateAltitude, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SPEED", std::bind(&MessageHandler::UpdateSpeed, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FLIGHTPLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_GROUND_STATUS", std::bind(&MessageHandler::UpdateAircraftStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DEPARTURE_INSTRUCTIONS", std::bind(&MessageHandler::UpdateDepartureInstructions, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ARRIVAL_INSTRUCTIONS", std::bind(&MessageHandler::UpdateArrivalInstructions, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_TRACKING_STATUS", std::bind(&MessageHandler::UpdateTrackingStatus, &messageHandler, std::placeholders::_1));

	// Interacts with SITU
	socketClient.socket()->on("REQUEST_RELEASE", std::bind(&MessageHandler::UpdateSitu, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("GRANT_RELEASE", std::bind(&MessageHandler::UpdateSitu, &messageHandler, std::placeholders::_1));

	//socketClient.socket()->on("UPDATE_ESTIMATE", std::bind(&MessageHandler::UpdateEstimate, &messageHandler, std::placeholders::_1));
	//socketClient.socket()->on("UPDATE_FLIGHT_PLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));
	//socketClient.socket()->on("UPDATE_DIRECT_TO", std::bind(&MessageHandler::UpdateDirectTo, &messageHandler, std::placeholders::_1));
	//socketClient.socket()->on("UPDATE_DESTINATION", std::bind(&MessageHandler::UpdateDestination, &messageHandler, std::placeholders::_1));
	//socketClient.socket()->on("TRIGGER_MISSED_APPROACH", std::bind(&MessageHandler::UpdateMissedApproach, &messageHandler, std::placeholders::_1));

	// EXCDS information requests
	socketClient.socket()->on("REQUEST_ALL_FP_DATA", std::bind(&MessageHandler::RequestAllAircraft, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_FP_DATA_CALLSIGN", std::bind(&MessageHandler::RequestAircraftByCallsign, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_FP", std::bind(&MessageHandler::RequestFlightPlan, &messageHandler, std::placeholders::_1));
}

/**
* Listens to flight data updates from EuroScope.
*/
void CEXCDSBridge::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	sio::message::ptr response = sio::object_message::create();
	response->get_map()["callsign"] = sio::string_message::create(FlightPlan.GetCallsign());
	MessageHandler::PrepareFlightPlanDataResponse(FlightPlan, response);

	GetSocket()->emit("SEND_FP_DATA", response);
}
void CEXCDSBridge::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType)
{
	sio::message::ptr response = sio::object_message::create();
	response->get_map()["callsign"] = sio::string_message::create(FlightPlan.GetCallsign());
	MessageHandler::PrepareFlightPlanDataResponse(FlightPlan, response);

	GetSocket()->emit("SEND_FP_DATA", response);
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
