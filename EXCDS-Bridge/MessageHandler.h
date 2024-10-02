#pragma once
#include "EuroScopePlugIn.h"

class MessageHandler
{
public:
	void UpdateAltitude(sio::event&);
	void UpdatePositions(sio::event& e);
	void SendKeyboardPresses(std::vector<WORD> message);
	void SendKeyboardString(const std::string str);
	void SendPDC(sio::event& e);
	void UpdateScratchPad(sio::event&);
	void UpdateRoute(sio::event& e);
	void UpdateSpeed(sio::event&);
	void UpdateFlightPlan(sio::event&);
	void UpdateAircraftStatus(sio::event&);
	void UpdateTrackingStatus(sio::event&);
	void UpdateSquawk(sio::event& e);
	void UpdateEstimate(sio::event&);
	void UpdateDirectTo(sio::event&);
	void UpdateTime(sio::event& e);
	void HandleNewFlightPlan(sio::event& e);

	static void RequestAirports(sio::message::ptr response);
	void RequestAllAircraft(sio::event&);
	void RequestAircraftByCallsign(sio::event&);
	static void PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response);
	static void PrepareRadarTargetResponse(EuroScopePlugIn::CRadarTarget rt, sio::message::ptr response);
	void PrepareRouteDataResponse(sio::event& e);
	void PrepareCDMResponse(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response);
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