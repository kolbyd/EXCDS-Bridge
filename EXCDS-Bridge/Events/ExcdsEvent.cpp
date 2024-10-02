#include "ExcdsEvent.h"
#include "EuroScopePlugIn.h"

std::string ExcdsEvent::GetCallsign(sio::event& event)
{
    return event.get_message()->get_map()["callsign"]->get_string();
}

std::map<std::string, sio::message::ptr> ExcdsEvent::GetMessageValue(sio::event& event)
{
    return event.get_message()->get_map();
}

void ExcdsEvent::SendNotModified(sio::event& event, std::string reason)
{
    _response->get_map()["modified"] = sio::bool_message::create(false);
    _response->get_map()["message"] = sio::string_message::create(reason);
    event.put_ack_message(_response);
}

void ExcdsEvent::SendModified(sio::event& event)
{
	_response->get_map()["modified"] = sio::bool_message::create(true);
	event.put_ack_message(_response);
}

void ExcdsEvent::TriggerEvent(sio::event& event)
{
	EuroScopePlugIn::CFlightPlan fp;
	if (!_skipFlightPlanChecks)
    {
        // Check if the flight plan exists (and is valid)

		fp = _bridgeInstance->FlightPlanSelect(GetCallsign(event).c_str());
		if (!fp.IsValid())
		{
			CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Cannot modify.", "NO_FPLN");
			SendNotModified(event, "Flight plan not found.");
			return;
		}

		// Are we allowed to modify this aircraft?
		if (!fp.GetTrackingControllerIsMe() && strlen(fp.GetTrackingControllerId()) != 0)
		{
			CEXCDSBridge::SendEuroscopeMessage(fp.GetCallsign(), "Cannot modify.", "ALRDY_TRACKD");
			SendNotModified(event, "Aircraft is being tracked by another controller.");

			return;
		}
    }

    ExecuteEvent(event, fp);
}

void ExcdsEvent::RegisterEvent(std::string eventName)
{
	_bridgeInstance->GetSocket()->on(eventName, [&](sio::event& ev)
	{
		TriggerEvent(ev);
	});
}