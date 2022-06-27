#include "pch.h"
#include "sio_client.h"

#include <afxsock.h>
#include <iostream>
#include "stdio.h"
#include "MessageHandler.h"
#include "CEXCDSBridge.h"

#define PLUGIN_NAME		"EXCDS Bridge"
#define PLUGIN_VERSION	"0.0.1-alpha"
#define PLUGIN_AUTHOR	"Kolby Dunning / Liam Shaw (Frontend)"
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
		"EXCDS Bridge", 
		"0.0.1-alpha",
		"Kolby Dunning / Liam Shaw (Frontend)", 
		"Attribution 4.0 International (CC BY 4.0)"
	)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	// Set instance
	instance = this;

	// Connect to server
	socketClient.connect(BRIDGE_HOST + ":" + BRIDGE_PORT);

	// Tell the program that we are connected
	socketClient.socket()->emit("CONNECTED", sio::message::list("true"));

	// TODO: Register for events
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

/**
* Creates the event listener for the plugin.
*/
void CEXCDSBridge::bind_events()
{
	MessageHandler messageHandler;

	socketClient.socket()->on("UPDATE_GROUND_STATUS", std::bind(&MessageHandler::UpdateGroundStatus, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_CLEARANCE", std::bind(&MessageHandler::UpdateClearance, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_TEMPORARY_ALTITUDE", std::bind(&MessageHandler::UpdateAltitude, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_FINAL_ALTITUDE", std::bind(&MessageHandler::UpdateAltitude, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_SQUAWK", std::bind(&MessageHandler::UpdateSquawk, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_DEPARTURE_RUNWAY", std::bind(&MessageHandler::UpdateRunway, &messageHandler, std::placeholders::_1));
	socketClient.socket()->on("UPDATE_ARRIVAL_RUNWAY", std::bind(&MessageHandler::UpdateRunway, &messageHandler, std::placeholders::_1));
}

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
