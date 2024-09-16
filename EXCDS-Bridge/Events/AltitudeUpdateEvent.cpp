#include "AltitudeUpdateEvent.h"
#include "../Response/ExcdsResponse.h"
#include "sio_client.h"

void AltitudeUpdateEvent::ExecuteEvent(sio::event& event, EuroScopePlugIn::CFlightPlan flightPlan)
{
	try {
		// Get aircraft data from EXCDS
		std::string callsign = flightPlan.GetCallsign();

		std::map<std::string, sio::message::ptr> e = GetMessageValue(event);
		std::string id = e["id"]->get_string();
		int cleared = e["cleared"]->get_int();
		int final = e["final"]->get_int();
		int coordinated = e["coordinated"]->get_int();
		// Reported altitude, goes to strip annotations for situ
		std::string reported = e["reported"]->get_string();

		if (cleared != -1) {
			flightPlan.GetControllerAssignedData().SetClearedAltitude(cleared);
		}
		/*
		if (final != -1) {
			fp.GetFlightPlanData().SetFinalAltitude(final);
			fp.GetControllerAssignedData().SetFinalAltitude(final);
		}
		*/

		if (coordinated != -1) {
			flightPlan.InitiateCoordination(flightPlan.GetCoordinatedNextController(), flightPlan.GetNextCopxPointName(), coordinated);
		}

		if (reported != "") {
			flightPlan.GetControllerAssignedData().SetFlightStripAnnotation(8, reported.c_str());
		}

		flightPlan.GetFlightPlanData().AmendFlightPlan();

		// Tell EXCDS the change is done
		SendModified(event);
	}
	catch (...) {
		CEXCDSBridge::SendEuroscopeMessage(flightPlan.GetCallsign(), ALT_EXCEPTION);
	}
}
