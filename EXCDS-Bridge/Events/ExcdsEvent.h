#pragma once

#include <string>
#include <map>
#include <sio_client.h>

#include "../CEXCDSBridge.h"

class ExcdsEvent
{
public:
    ExcdsEvent() {};
    ExcdsEvent(bool skipFlightPlanChecks): _skipFlightPlanChecks(skipFlightPlanChecks) {};

    /**
    * The EXCDS Bridge will call this method when an event is received from the socket.
    */
    void TriggerEvent(sio::event&);
    void RegisterEvent(std::string);
protected:
    CEXCDSBridge* _bridgeInstance = CEXCDSBridge::GetInstance();
    sio::message::ptr _response = sio::object_message::create();

    /**
    * This method is called internally by TriggerEvent. It is where the event is actually executed.
    */
    virtual void ExecuteEvent(sio::event&, EuroScopePlugIn::CFlightPlan) = 0;

    /**
    * The following functions are used to extract the callsign and value from the payload.
    */

    std::string GetCallsign(sio::event&);
    std::map<std::string, sio::message::ptr> GetMessageValue(sio::event&);

    void SendNotModified(sio::event&, std::string);
    void SendModified(sio::event&);
private:
    bool _skipFlightPlanChecks = false;
};

