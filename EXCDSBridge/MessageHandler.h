#pragma once
#include "EuroScopePlugIn.h"

class MessageHandler
{
public:
	void UpdateGroundStatus(sio::event &);
	void UpdateClearance(sio::event&);
	void UpdateAltitude(sio::event&);
	void UpdateSquawk(sio::event&);
	void UpdateRunway(sio::event&);
private:
	bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response, sio::event& e);
	sio::message::ptr NotModified(sio::message::ptr response, std::string reason);
	std::string AddRunwayToRoute(std::string runway, EuroScopePlugIn::CFlightPlan fp, bool departure = true);
};

