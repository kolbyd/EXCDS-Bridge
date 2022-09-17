#pragma once
#include "EuroScopePlugIn.h"

class MessageHandler
{
public:
	void UpdateAltitude(sio::event&);
	void UpdateScratchPad(sio::event&);
	void UpdateSquawk(sio::event&);
	void UpdateSpeed(sio::event&);
	void UpdateRunway(sio::event&);
	void UpdateSitu(sio::event&);
	void UpdateStripAnnotation(sio::event&);
	void UpdateFlightPlan(sio::event&);
	//void UpdateEstimate(sio::event&);
	//void UpdateFlightPlan(sio::event&);
	//void UpdateDirectTo(sio::event&);
	//void UpdateDestination(sio::event&);
	//void UpdateMissedApproach(sio::event&);

	void RequestAllAircraft(sio::event&);
	void RequestAircraftByCallsign(sio::event&);
	static void PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response);
	static void PrepareControllerDataReponse(sio::message::ptr response);
private:
	bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response, sio::event& e);
	sio::message::ptr NotModified(sio::message::ptr response, std::string reason);
	std::string AddRunwayToRoute(std::string runway, EuroScopePlugIn::CFlightPlan fp, bool departure = true);
	static std::string SquawkGenerator(std::string);
	void DirectTo(std::string waypoint, EuroScopePlugIn::CFlightPlan fp);
};

