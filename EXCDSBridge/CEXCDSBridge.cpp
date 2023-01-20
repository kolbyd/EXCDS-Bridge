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

void CEXCDSBridge::OnTimer(int Counter)
{
		CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
		EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();
		EuroScopePlugIn::CRadarTarget radarTarget = bridgeInstance->RadarTargetSelectFirst();
	
		while (flightPlan.IsValid()) {
			if (flightPlan.GetState() > 0)
			{
				sio::message::ptr response = sio::object_message::create();
				MessageHandler::PrepareFlightPlanDataResponse(flightPlan, response);
	
				bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
			}
	
			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
		}

		while (radarTarget.IsValid()) {
			sio::message::ptr response = sio::object_message::create();
			MessageHandler::PrepareTargetResponse(radarTarget, response);

			bridgeInstance->GetSocket()->emit("SEND_RT_DATA", response);


			radarTarget = bridgeInstance->RadarTargetSelectNext(radarTarget);
		}
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
