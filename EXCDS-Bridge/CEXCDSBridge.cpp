#include "sio_client.h"

#include <afxsock.h>
#include <iostream>
#include <string>
#include "stdio.h"
#include "MessageHandler.h"
#include "CEXCDSBridge.h"
#include "BaseMessage.h"

#define PLUGIN_NAME		"EXCDS Bridge"
#define PLUGIN_VERSION	"0.0.5-alpha"
#define PLUGIN_AUTHOR	"Kolby Dunning / Liam Shaw"
#define PLUGIN_LICENSE	"Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)"

const std::string BRIDGE_HOST = "http://127.0.0.1";
const std::string BRIDGE_PORT = "7501";

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
	socketClient.socket()->on("UPDATE_TIME", std::bind(&MessageHandler::UpdateTime, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ALTITUDE", std::bind(&MessageHandler::UpdateAltitude, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SPEED", std::bind(&MessageHandler::UpdateSpeed, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_STATUS", std::bind(&MessageHandler::UpdateAircraftStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_TRACKING_STATUS", std::bind(&MessageHandler::UpdateTrackingStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DIRECT", std::bind(&MessageHandler::UpdateDirectTo, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FLIGHT_PLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SCRATCHPAD", std::bind(&MessageHandler::UpdateScratchPad, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ROUTE", std::bind(&MessageHandler::UpdateRoute, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("NEW_FLIGHT_PLAN", std::bind(&MessageHandler::HandleNewFlightPlan, &messageHandler, std::placeholders::_1));

	// EXCDS information requests
	socketClient.socket()->on("REQUEST_ALL_FP_DATA", std::bind(&MessageHandler::RequestAllAircraft, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_FP_DATA_CALLSIGN", std::bind(&MessageHandler::RequestAircraftByCallsign, &messageHandler, std::placeholders::_1));
}

void CEXCDSBridge::OnTimer(int counter)
{
	if (counter % 5 != 0) return;

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();


	// @see https://github.com/socketio/socket.io-client-cpp/issues/263
	// Iterate over all the flight plans ES has
	sio::message::ptr arrayMessage = sio::array_message::create();

	while (flightPlan.IsValid()) {
		// If the FP is in an FLIGHT_PLAN_STATE_NON_CONCERNED or FLIGHT_PLAN_STATE_NOTIFIED state, we don't need this data
		if (flightPlan.GetState() < 1)
		{
			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
			continue;
		}

		// Create a new object message and store it
		sio::message::ptr msg = sio::object_message::create();
		MessageHandler::PrepareFlightPlanDataResponse(flightPlan, msg);

		arrayMessage->get_vector().push_back(msg);

		flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
	}

	// Send
	bridgeInstance->GetSocket()->emit("MASS_SEND_FP_DATA", arrayMessage);

	//EuroScopePlugIn::CController controller = bridgeInstance->ControllerSelectFirst();
	//while (controller.IsValid())
	//{
	//	std::string controllerId = controller.GetPositionId();
	//	std::string controllerCallsign = controller.GetCallsign();
	//	double controllerFrequency = controller.GetPrimaryFrequency();

	//	response->get_map()[controllerId] = sio::object_message::create();
	//	response->get_map()[controllerId]->get_map()["callsign"] = sio::string_message::create(controllerCallsign);
	//	response->get_map()[controllerId]->get_map()["frequency"] = sio::double_message::create(controllerFrequency);

	//	controller = bridgeInstance->ControllerSelectNext(controller);
	//}

	//bridgeInstance->GetSocket()->emit("SEND_CTRLR_DATA", response);
}

void CEXCDSBridge::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan fp, int Datatype)
{
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
}

void CEXCDSBridge::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan fp)
{
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
}

void CEXCDSBridge::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget rt)
{
	sio::message::ptr response = sio::object_message::create();
	MessageHandler::PrepareRadarTargetResponse(rt, response);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_RT_DATA", response);

	rt = bridgeInstance->RadarTargetSelectNext(rt);

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

	std::string output = std::string(message) + " (ID: " + std::string(id) + ")";

	GetInstance()->DisplayUserMessage(
		"EXCDS Bridge",
		callsign,
		output.c_str(),
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
