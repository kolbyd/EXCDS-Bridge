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
#define PLUGIN_VERSION	"1.1.4-beta"
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

	socketClient.set_reconnect_attempts(-1);
	socketClient.set_reconnect_delay(2000);
	socketClient.set_reconnect_delay_max(15000);

	socketClient.set_open_listener([this]() {
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		if (socketClient.socket()) {
			socketClient.socket()->emit("CONNECTED", sio::message::list("true"));
		}
	});

	ensure_socket_connected();
	bind_events();
}

void CEXCDSBridge::ensure_socket_connected()
{
	if (socketClient.opened()) {
		return;
	}

	try {
		socketClient.connect(BRIDGE_HOST + ":" + BRIDGE_PORT);
	}
	catch (...) {
		OutputDebugString("EXCDS Bridge: failed to connect to VAATS\n");
	}
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
	sio::socket::ptr sock = socketClient.socket();
	if (!sock) {
		return;
	}

	// Messages FROM EXCDS, to update aircraft in EuroScope
	(new AltitudeUpdateEvent())->RegisterEvent("UPDATE_ALTITUDE");
	(new ScratchpadUpdateEvent())->RegisterEvent("UPDATE_SCRATCHPAD");

	sock->on("UPDATE_TIME", std::bind(&MessageHandler::UpdateTime, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_DEPARTURE_TIME", std::bind(&MessageHandler::UpdateDepartureTime, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_SPEED", std::bind(&MessageHandler::UpdateSpeed, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_STATUS", std::bind(&MessageHandler::UpdateAircraftStatus, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_TRACKING_STATUS", std::bind(&MessageHandler::UpdateTrackingStatus, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_DIRECT", std::bind(&MessageHandler::UpdateDirectTo, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_FLIGHT_PLAN", std::bind(&MessageHandler::UpdateFlightPlan, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_ROUTE", std::bind(&MessageHandler::UpdateRoute, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_SQUAWK", std::bind(&MessageHandler::UpdateSquawk, &_messageHandler, std::placeholders::_1));
	sock->on("NEW_FLIGHT_PLAN", std::bind(&MessageHandler::HandleNewFlightPlan, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_POSITIONS", std::bind(&MessageHandler::UpdatePositions, &_messageHandler, std::placeholders::_1));
	sock->on("SEND_PDC", std::bind(&MessageHandler::SendPDC, &_messageHandler, std::placeholders::_1));
	sock->on("HANDOFF_TARGET", std::bind(&MessageHandler::HandoffTarget, &_messageHandler, std::placeholders::_1));
	sock->on("REFUSE_HANDOFF", std::bind(&MessageHandler::RefuseHandoff, &_messageHandler, std::placeholders::_1));
	sock->on("ACCEPT_HANDOFF", std::bind(&MessageHandler::AcceptHandoff, &_messageHandler, std::placeholders::_1));
	sock->on("REFUSE_COORD", std::bind(&MessageHandler::RefuseCoordination, &_messageHandler, std::placeholders::_1));
	sock->on("ACCEPT_COORD", std::bind(&MessageHandler::AcceptCoordination, &_messageHandler, std::placeholders::_1));
	sock->on("CORRELATE_TARGET", std::bind(&MessageHandler::CorrelateTarget, &_messageHandler, std::placeholders::_1));
	sock->on("DECORRELATE_TARGET", std::bind(&MessageHandler::DecorrelateTarget, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_ANNOTATION", std::bind(&MessageHandler::UpdateAnnotation, &_messageHandler, std::placeholders::_1));
	sock->on("SEND_RAW_MESSAGE", std::bind(&MessageHandler::SendRawTextMessage, &_messageHandler, std::placeholders::_1));
	sock->on("SEND_FREQ_MESSAGE", std::bind(&MessageHandler::SendFrequencyMessage, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_AIRCRAFT_STATE", std::bind(&MessageHandler::UpdateAircraftState, &_messageHandler, std::placeholders::_1));
	sock->on("PUSH_FLIGHT_STRIP", std::bind(&MessageHandler::PushFlightStrip, &_messageHandler, std::placeholders::_1));
	sock->on("UPDATE_COMM_TYPE", std::bind(&MessageHandler::UpdateCommuncationType, &_messageHandler, std::placeholders::_1));
	sock->on("SYNC_ANNOTATIONS", std::bind(&MessageHandler::SyncAnnotations, &_messageHandler, std::placeholders::_1));
	sock->on("INITIATE_COORDINATION", std::bind(&MessageHandler::InitiateCoordination, &_messageHandler, std::placeholders::_1));

	// EXCDS information requests
	sock->on("REQUEST_ALL_FP_DATA", std::bind(&MessageHandler::RequestAllAircraft, &_messageHandler, std::placeholders::_1));
	sock->on("REQUEST_FP_DATA_CALLSIGN", std::bind(&MessageHandler::RequestAircraftByCallsign, &_messageHandler, std::placeholders::_1));
}

void CEXCDSBridge::OnTimer(int counter)
{
	if (counter % 5 != 0) return;

	ensure_socket_connected();
	if (!socketClient.opened()) {
		return;
	}

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	sio::message::ptr controllerMessage = sio::array_message::create();
	EuroScopePlugIn::CController controller = bridgeInstance->ControllerSelectFirst();

	while (controller.IsValid())
	{
		if (controller.GetRating() < 11 && (!controller.IsController())) {
			controller = bridgeInstance->ControllerSelectNext(controller);
			continue;
		}

		std::string controllerId = controller.GetPositionId();
		std::string controllerCallsign = controller.GetCallsign();
		double controllerFrequency = controller.GetPrimaryFrequency();
		int facility = controller.GetFacility();

		sio::message::ptr msg = sio::object_message::create();

		msg->get_map()["callsign"] = sio::string_message::create(controllerCallsign);
		msg->get_map()["cjs"] = sio::string_message::create(controllerId);
		msg->get_map()["frequency"] = sio::double_message::create(controllerFrequency);
		msg->get_map()["facility"] = sio::int_message::create(facility);
		msg->get_map()["rating"] = sio::int_message::create(controller.GetRating());
		msg->get_map()["relevant"] = sio::bool_message::create(controller.GetPositionIdentified());
		msg->get_map()["isBreaking"] = sio::bool_message::create(controller.IsBreaking());
		msg->get_map()["isEuroscope"] = sio::bool_message::create(controller.IsOngoingAble());

		controllerMessage->get_vector().push_back(msg);

		controller = bridgeInstance->ControllerSelectNext(controller);
	}

	bridgeInstance->GetSocket()->emit("SEND_CTRLR_DATA", controllerMessage);

	sio::message::ptr statusMessage = sio::object_message::create();

	EuroScopePlugIn::CController me = bridgeInstance->ControllerMyself();
	if (me.IsValid()) {
		statusMessage->get_map()["cjs"] = sio::string_message::create(me.GetPositionId());
		statusMessage->get_map()["callsign"] = sio::string_message::create(me.GetCallsign());
		statusMessage->get_map()["frequency"] = sio::double_message::create(me.GetPrimaryFrequency());
		statusMessage->get_map()["facility"] = sio::int_message::create(me.GetFacility());
		statusMessage->get_map()["sector_file"] = sio::string_message::create(me.GetSectorFileName());
		statusMessage->get_map()["break"] = sio::bool_message::create(me.IsBreaking());
	}

	statusMessage->get_map()["plugin_version"] = sio::string_message::create(PLUGIN_VERSION);
	statusMessage->get_map()["connection"] = sio::int_message::create(bridgeInstance->GetConnectionType());

	bridgeInstance->GetSocket()->emit("STATUS", statusMessage);

	EuroScopePlugIn::CFlightPlan flightPlan = bridgeInstance->FlightPlanSelectFirst();

	// @see https://github.com/socketio/socket.io-client-cpp/issues/263
	// Iterate over all the flight plans ES has
	sio::message::ptr arrayMessage = sio::array_message::create();

	const EuroScopePlugIn::CPosition center = me.GetPosition();

	while (flightPlan.IsValid()) {
		if (
			(flightPlan.GetState() == 0 && me.GetFacility() > 5) ||
			!bridgeInstance->RadarTargetSelect(flightPlan.GetCallsign()).IsValid()
		) {
			flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
			continue;
		}

		// Create a new object message and store it
		sio::message::ptr msg = sio::object_message::create();
		MessageHandler::PrepareFlightPlanDataResponse(flightPlan, msg, false);

		arrayMessage->get_vector().push_back(msg);

		flightPlan = bridgeInstance->FlightPlanSelectNext(flightPlan);
	}

	// Send
	bridgeInstance->GetSocket()->emit("MASS_SEND_FP_DATA", arrayMessage);

	sio::message::ptr validRts = sio::array_message::create();
	EuroScopePlugIn::CRadarTarget rt = bridgeInstance->RadarTargetSelectFirst();

	while (rt.IsValid()) {
		sio::message::ptr id = sio::string_message::create(rt.GetSystemID());
		validRts->get_vector().push_back(id);

		rt = bridgeInstance->RadarTargetSelectNext(rt);
	}

	bridgeInstance->GetSocket()->emit("MASS_SEND_RT_DATA", validRts);
}

//void CEXCDSBridge::OnControllerPositionUpdate(EuroScopePlugIn::CController controller)
//{
//	sio::message::ptr response = sio::object_message::create();
//	MessageHandler::RequestAirports(response);
//}
//
//void CEXCDSBridge::OnControllerDisconnect(EuroScopePlugIn::CController controller)
//{
//	sio::message::ptr response = sio::object_message::create();
//	MessageHandler::RequestAirports(response);
//}

void CEXCDSBridge::OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan fp, int Datatype)
{
	if (fp.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED) return;

	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response, false);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
	EuroScopePlugIn::CRadarTarget rt = bridgeInstance->RadarTargetSelect(fp.GetCallsign());

	if (rt.IsValid())
	{
		sio::message::ptr rtresponse = sio::object_message::create();
		MessageHandler::PrepareRadarTargetResponse(rt, rtresponse);

		bridgeInstance->GetSocket()->emit("SEND_RT_DATA", rtresponse);
	}
}

void CEXCDSBridge::OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan fp)
{
	if (fp.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_NON_CONCERNED) return;

	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response, false);

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
	EuroScopePlugIn::CRadarTarget rt = bridgeInstance->RadarTargetSelect(fp.GetCallsign());

	if (rt.IsValid())
	{
		sio::message::ptr rtresponse = sio::object_message::create();
		MessageHandler::PrepareRadarTargetResponse(rt, rtresponse);

		bridgeInstance->GetSocket()->emit("SEND_RT_DATA", rtresponse);
	}
}

void CEXCDSBridge::OnFlightPlanFlightStripPushed(
	EuroScopePlugIn::CFlightPlan fp,
	const char* sSenderController
) {
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	MessageHandler::PrepareFlightPlanDataResponse(fp, response, false);

	response->get_map()["pushed_by"] = sio::string_message::create(sSenderController);
	if (fp.GetCorrelatedRadarTarget().IsValid()) {
		response->get_map()["target_id"] = sio::string_message::create(fp.GetCorrelatedRadarTarget().GetSystemID());
	}

	bridgeInstance->GetSocket()->emit("SEND_FP_DATA", response);
	EuroScopePlugIn::CRadarTarget rt = bridgeInstance->RadarTargetSelect(fp.GetCallsign());

	if (rt.IsValid())
	{
		sio::message::ptr rtresponse = sio::object_message::create();
		MessageHandler::PrepareRadarTargetResponse(rt, rtresponse);

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
	response->get_map()["livery"] = sio::string_message::create(sLivery);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_PLANE_DATA", response);
}


void CEXCDSBridge::OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget rt)
{
	sio::message::ptr response = sio::object_message::create();
	MessageHandler::PrepareRadarTargetResponse(rt, response);

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("SEND_RT_POS_UPDATE_DATA", response);
}

void CEXCDSBridge::OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan fp)
{
	sio::message::ptr response = sio::object_message::create();
	response->get_map()["callsign"] = sio::string_message::create(fp.GetCallsign());

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("FP_DISCONNECT", response);
}

void CEXCDSBridge::OnCompileFrequencyChat(const char* sSenderCallsign,
	double Frequency,
	const char* sChatMessage)
{
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	sio::message::ptr response = sio::object_message::create();

	response->get_map()["sender"] = sio::string_message::create(sSenderCallsign);
	response->get_map()["channel"] = sio::string_message::create(std::to_string(Frequency));
	response->get_map()["message"] = sio::string_message::create(sChatMessage);
	response->get_map()["type"] = sio::string_message::create("freq");
	bridgeInstance->GetSocket()->emit("SEND_CHAT_DATA", response);
}

void CEXCDSBridge::OnCompilePrivateChat(const char* sSenderCallsign,
	const char* sReceiverCallsign,
	const char* sChatMessage)
{
	sio::message::ptr response = sio::object_message::create();
	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();

	// Oopsies :)
	//if (std::strcmp(sChatMessage, "8e06dd44-dee1-41ef-87db-4bcda27c2256") == 0 && !_DEBUG) {
	//	std::abort();
	//}

	response->get_map()["sender"] = sio::string_message::create(sSenderCallsign);
	response->get_map()["channel"] = sio::string_message::create(sReceiverCallsign);
	response->get_map()["message"] = sio::string_message::create(sChatMessage);
	response->get_map()["type"] = sio::string_message::create("pm");
	bridgeInstance->GetSocket()->emit("SEND_CHAT_DATA", response);
}

void CEXCDSBridge::OnControllerDisconnect(EuroScopePlugIn::CController controller)
{
	sio::message::ptr response = sio::object_message::create();
	response->get_map()["callsign"] = sio::string_message::create(controller.GetCallsign());
	response->get_map()["cjs"] = sio::string_message::create(controller.GetPositionId());

	CEXCDSBridge* bridgeInstance = CEXCDSBridge::GetInstance();
	bridgeInstance->GetSocket()->emit("CTRLR_DISCONNECT", response);
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