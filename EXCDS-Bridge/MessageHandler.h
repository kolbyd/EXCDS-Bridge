#pragma once
#include "EuroScopePlugIn.h"

class MessageHandler
{
public:
	void UpdateAltitude(sio::event&);
	void UpdateScratchPad(sio::event&);
	void UpdateRoute(sio::event& e);
	void UpdateSpeed(sio::event&);
	void UpdateRunway(sio::event&);
	void UpdateSitu(sio::event&);
	void UpdateFlightPlan(sio::event&);
	void UpdateAircraftStatus(sio::event&);
	void UpdateDepartureInstructions(sio::event&);
	void UpdateArrivalInstructions(sio::event&);
	void UpdateTrackingStatus(sio::event&);
	void UpdateEstimate(sio::event&);
	void UpdateDirectTo(sio::event&);
	void UpdateTime(sio::event& e);
	void HandleNewFlightPlan(sio::event& e);

	void RequestAllAircraft(sio::event&);
	void RequestAircraftByCallsign(sio::event&);
	static void PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response);
	static void PrepareRadarTargetResponse(EuroScopePlugIn::CRadarTarget rt, sio::message::ptr response);
	void RequestDirectTo(sio::event&);

private:
	bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response, sio::event& e);
	sio::message::ptr NotModified(sio::message::ptr response, std::string reason);
	std::string AddRunwayToRoute(std::string runway, EuroScopePlugIn::CFlightPlan fp, bool departure = true);
	static std::string SquawkGenerator(std::string);
	void ForceFlightPlanRefresh(EuroScopePlugIn::CFlightPlan fp);
	void DirectTo(std::string waypoint, EuroScopePlugIn::CFlightPlan fp, bool newRoute);
	bool StatusAssign(std::string status, EuroScopePlugIn::CFlightPlan fp, std::string departureTime);
	void MissedApproach(EuroScopePlugIn::CFlightPlan fp);
};