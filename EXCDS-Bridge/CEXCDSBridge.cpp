#include "sio_client.h"

#include <afxsock.h>
#include <iostream>
#include <string>
#include "stdio.h"
#include "MessageHandler.h"
#include "CEXCDSBridge.h"

// Events
#include "Events/AltitudeUpdateEvent.h"
#include "Events/ScratchpadUpdateEvent.h"

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
	(new AltitudeUpdateEvent())->RegisterEvent("UPDATE_ALTITUDE");
	(new ScratchpadUpdateEvent())->RegisterEvent("UPDATE_SCRATCHPAD");

	socketClient.socket()->on("UPDATE_TIME", std::bind(&MessageHandler::UpdateTime, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DEPARTURE_TIME", std::bind(&MessageHandler::UpdateDepartureTime, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SPEED", std::bind(&MessageHandler::UpdateSpeed, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_STATUS", std::bind(&MessageHandler::UpdateAircraftStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_TRACKING_STATUS", std::bind(&MessageHandler::UpdateTrackingStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DIRECT", std::bind(&MessageHandler::UpdateDirectTo, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FLIGHT_PLAN", std::bind(&MessageHandler::UpdateFlightPlan, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ROUTE", std::bind(&MessageHandler::UpdateRoute, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SQUAWK", std::bind(&MessageHandler::UpdateSquawk, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("NEW_FLIGHT_PLAN", std::bind(&MessageHandler::HandleNewFlightPlan, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_POSITIONS", std::bind(&MessageHandler::UpdatePositions, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("SEND_PDC", std::bind(&MessageHandler::SendPDC, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("HANDOFF_TARGET", std::bind(&MessageHandler::HandoffTarget, &messageHandler, std::placeholders::_1));

	// EXCDS information requests
	socketClient.socket()->on("REQUEST_ALL_FP_DATA", std::bind(&MessageHandler::RequestAllAircraft, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_FP_DATA_CALLSIGN", std::bind(&MessageHandler::RequestAircraftByCallsign, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("REQUEST_ROUTE_DATA", std::bind(&MessageHandler::PrepareRouteDataResponse, &messageHandler, std::placeholders::_1));
}

void CEXCDSBridge::OnTimer(int counter)
{
	if (counter % 5 != 0) return;

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

	// @see https://github.com/socketio/socket.io-client-cpp/issues/263
	// Iterate over all the flight plans ES has
	sio::message::ptr arrayMessage = sio::array_message::create();
	sio::message::ptr extrapolated = sio::array_message::create();

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

		if (!flightPlan.GetCorrelatedRadarTarget().IsValid() && flightPlan.GetFPTrackPosition().IsValid())
		{
			EuroScopePlugIn::CRadarTargetPositionData rt = flightPlan.GetFPTrackPosition();
			EuroScopePlugIn::CFlightPlan fp = flightPlan;

			sio::message::ptr trackMsg = sio::object_message::create();

			double lat = rt.GetPosition().m_Latitude;
			double lon = rt.GetPosition().m_Longitude;

			trackMsg->get_map()["position"] = sio::object_message::create();
			trackMsg->get_map()["position"]->get_map()["lat"] = sio::double_message::create(lat);
			trackMsg->get_map()["position"]->get_map()["lon"] = sio::double_message::create(lon);
		}
	}

	// Send

	sio::message::ptr controllerMessage = sio::array_message::create();

	EuroScopePlugIn::CController controller = bridgeInstance->ControllerSelectFirst();

	EuroScopePlugIn::CController me = bridgeInstance->ControllerMyself();

	while (controller.IsValid())
	{
		if (!controller.IsController())
			controller = bridgeInstance->ControllerSelectNext(controller);

		std::string controllerId = controller.GetPositionId();
		std::string controllerCallsign = controller.GetCallsign();
		double controllerFrequency = controller.GetPrimaryFrequency();
		int facility = controller.GetFacility();
		bool isMe = strcmp(controller.GetCallsign(), me.GetCallsign()) == 0;

		sio::message::ptr msg = sio::object_message::create();

		msg->get_map()["callsign"] = sio::string_message::create(controllerCallsign);
		msg->get_map()["cjs"] = sio::string_message::create(controllerId);
		msg->get_map()["frequency"] = sio::double_message::create(controllerFrequency);
		msg->get_map()["facility"] = sio::int_message::create(facility);
		msg->get_map()["isMe"] = sio::bool_message::create(isMe);

		if (controller.GetFacility() > 0)
			controllerMessage->get_vector().push_back(msg);

		controller = bridgeInstance->ControllerSelectNext(controller);
	}

	bridgeInstance->GetSocket()->emit("SEND_CTRLR_DATA", controllerMessage);
	bridgeInstance->GetSocket()->emit("MASS_SEND_FP_DATA", arrayMessage);

	sio::message::ptr statusMessage = sio::object_message::create();

	if (me.IsValid()) {
		statusMessage->get_map()["cjs"] = sio::string_message::create(me.GetPositionId());
		statusMessage->get_map()["callsign"] = sio::string_message::create(me.GetCallsign());
		statusMessage->get_map()["freq"] = sio::double_message::create(me.GetPrimaryFrequency());
	}

	statusMessage->get_map()["connection"] = sio::int_message::create(bridgeInstance->GetConnectionType());

		bridgeInstance->GetSocket()->emit("STATUS", statusMessage);
}

void CEXCDSBridge::OnControllerPositionUpdate(EuroScopePlugIn::CController controller)
{
	sio::message::ptr response = sio::object_message::create();
	MessageHandler::RequestAirports(response);
}

void CEXCDSBridge::OnControllerDisconnect(EuroScopePlugIn::CController controller)
{
	sio::message::ptr response = sio::object_message::create();
	MessageHandler::RequestAirports(response);
}

void CEXCDSBridge::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan fp, int Datatype)
{
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);

	if (fp.GetCorrelatedRadarTarget().IsValid())
	{
		sio::message::ptr rtresponse = sio::object_message::create();
		MessageHandler::PrepareRadarTargetResponse(fp.GetCorrelatedRadarTarget(), rtresponse);

		bridgeInstance->GetSocket()->emit("SEND_RT_DATA", rtresponse);
	}
}

void CEXCDSBridge::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan fp)
{
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);

	if (fp.GetCorrelatedRadarTarget().IsValid())
	{
		sio::message::ptr rtresponse = sio::object_message::create();
		MessageHandler::PrepareRadarTargetResponse(fp.GetCorrelatedRadarTarget(), rtresponse);

		bridgeInstance->GetSocket()->emit("SEND_RT_DATA", rtresponse);
	}
}

void CEXCDSBridge::OnPlaneInformationUpdate(const char* sCallsign,
	const char* sLivery,
	const char* sPlaneType)
{
	sio::message::ptr response = sio::object_message::create();

	response->get_map()["callsign"] = sio::string_message::create(sCallsign);
	response->get_map()["type"] = sio::string_message::create(sPlaneType);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_PLANE_DATA", response);
}

void CEXCDSBridge::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget rt)
{
	sio::message::ptr response = sio::object_message::create();
	MessageHandler::PrepareRadarTargetResponse(rt, response);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_RT_DATA", response);
}

void CEXCDSBridge::OnCompileFrequencyChat(const char* sSenderCallsign,
	double Frequency,
	const char* sChatMessage)
{
	sio::message::ptr response = sio::object_message::create();

	response->get_map()["sender"] = sio::string_message::create(sSenderCallsign);
	response->get_map()["message"] = sio::string_message::create(sChatMessage);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_CHAT_DATA", response);
}

void CEXCDSBridge::OnCompilePrivateChat(const char* sSenderCallsign,
	const char* sReceiverCallsign,
	const char* sChatMessage)
{
	sio::message::ptr response = sio::object_message::create();

	response->get_map()["sender"] = sio::string_message::create(sSenderCallsign);
	response->get_map()["message"] = sio::string_message::create(sChatMessage);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_CHAT_DATA", response);
}

void CEXCDSBridge::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan)
{
	GetSocket()->emit("CALLSIGN_DISCONNECT", sio::string_message::create(FlightPlan.GetCallsign()));
}

/**
* Helper methods
*/
void CEXCDSBridge::SendEuroscopeMessage(const char* callsign, const char* message, const char* id)
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

void CEXCDSBridge::SendEuroscopeMessage(const char* callsign, ExcdsResponseType type)
{
	ExcdsResponse response(type);

	SendEuroscopeMessage(callsign, response.GetExcdsMessage().c_str(), response.GetCode().c_str());
}

CEXCDSBridge* CEXCDSBridge::GetInstance()
{
	return instance;
}

sio::socket::ptr CEXCDSBridge::GetSocket()
{
	return socketClient.socket();
}