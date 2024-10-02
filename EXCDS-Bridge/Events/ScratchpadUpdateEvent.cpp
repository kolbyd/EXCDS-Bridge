#include "ScratchpadUpdateEvent.h"
#include "../Response/ExcdsResponse.h"
#include "sio_client.h"

/**
* Event payload:
* 
* {
*	"callsign": "AAL123",
*	"value": "New scratchpad value."
* }
*/

void ScratchpadUpdateEvent::ExecuteEvent(sio::event& event, EuroScopePlugIn::CFlightPlan flightPlan)
{
	// The value to set the scratchpad to
    std::string value = GetMessageValue(event)["value"]->get_string();

	bool isAssigned = false;
	try {
		isAssigned = flightPlan.GetControllerAssignedData().SetScratchPadString(value.c_str());
	}
	catch (...) {
		SendNotModified(event, "Exception thrown when attempting to modify scratchpad.");

		CEXCDSBridge::SendEuroscopeMessage(flightPlan.GetCallsign(), SCRCHPD_STRNG_NOT_SET);
		return;
	}

	if (!isAssigned) {
		SendNotModified(event, "The scratchpad was not modified.");

		CEXCDSBridge::SendEuroscopeMessage(flightPlan.GetCallsign(), SCRCHPD_NOT_MODIFIED);
		return;
	}

	// Tell EXCDS the change is done
	SendModified(event);
}