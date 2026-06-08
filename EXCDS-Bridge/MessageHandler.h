#pragma once
#include "EuroScopePlugIn.h"

class MessageHandler
{
public:
	void UpdateAltitude(sio::event&);
	void InitiateCoordination(sio::event& e);
	void UpdatePositions(sio::event& e);
	void SendKeyboardPresses(const std::vector<WORD>& message);
	void SendKeyboardString(const std::string& str);
	void SendPDC(sio::event& e);
	void SendFrequencyMessage(sio::event& e);
	void SendRawTextMessage(sio::event& e);
	void RequestSectorData(sio::event& e);
	void HandoffTarget(sio::event& e);
	void RefuseHandoff(sio::event& e);
	void AcceptHandoff(sio::event& e);
	void RefuseCoordination(sio::event& e);
	void AcceptCoordination(sio::event& e);
	void CorrelateTarget(sio::event& e);
	void DecorrelateTarget(sio::event& e);
	void UpdateScratchPad(sio::event&);
	void UpdateRoute(sio::event& e);
	void UpdateAircraftState(sio::event& e);
	void UpdateDepartureTime(sio::event& e);
	void UpdateSpeed(sio::event&);
	void UpdateFlightPlan(sio::event&);
	void UpdateAircraftStatus(sio::event&);
	void UpdateTrackingStatus(sio::event&);
	void PushFlightStrip(sio::event& e);
	void UpdateAnnotation(sio::event& e);
	void UpdateSquawk(sio::event& e);
	void UpdateEstimate(sio::event&);
	void UpdateDirectTo(sio::event&);
	void UpdateTime(sio::event& e);
	void UpdateCommuncationType(sio::event& e);
	void HandleNewFlightPlan(sio::event& e);
	void SyncAnnotations(sio::event& e);
	static void RequestAirports(sio::message::ptr response);
	void RequestAllAircraft(sio::event&);
	void RequestAircraftByCallsign(sio::event&);
	static bool PrepareFlightPlanDataResponse(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response, boolean full);
	static void PrepareRadarTargetResponse(EuroScopePlugIn::CRadarTarget rt, sio::message::ptr response);
	static void PrepareRadarTargetPositionResponse(EuroScopePlugIn::CRadarTarget rt, sio::message::ptr response);
	static void AppendFlightPlanEsTracking(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response);
	static void EmitFlightPlanPatch(EuroScopePlugIn::CFlightPlan fp, const char* eventName, int controllerDataType = -1, const char* pushedBy = nullptr, const char* pushedTo = nullptr);
	void RequestDirectTo(sio::event&);

private:
	bool MessageHandler::FlightPlanChecks(EuroScopePlugIn::CFlightPlan fp, sio::message::ptr response, sio::event& e);
	sio::message::ptr NotModified(sio::message::ptr response, std::string reason);
	std::string AddRunwayToRoute(std::string runway, EuroScopePlugIn::CFlightPlan fp, bool departure = true);
	static std::string SquawkGenerator(std::string);
	void DirectTo(std::string waypoint, EuroScopePlugIn::CFlightPlan fp, bool newRoute);
	bool StatusAssign(std::string status, EuroScopePlugIn::CFlightPlan fp, std::string departureTime);
};