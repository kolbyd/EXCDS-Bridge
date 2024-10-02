#pragma once
#include "sio_client.h"
#include "EuroScopePlugIn.h"

#include "Response/ExcdsResponse.h"

class CEXCDSBridge : public EuroScopePlugIn::CPlugIn
{
public:
    static CEXCDSBridge* GetInstance();
    static sio::socket::ptr GetSocket();
    static void SendEuroscopeMessage(const char* callsign, const char* message, const char* id);
    static void CEXCDSBridge::SendEuroscopeMessage(const char*, ExcdsResponseType);

    CEXCDSBridge();
    virtual ~CEXCDSBridge();

    // Listens to EuroScope
    void OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan);
    void OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType);
    void OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan);
    void OnTimer(int Counter);
    void OnControllerPositionUpdate(EuroScopePlugIn::CController controller);
    void OnControllerDisconnect(EuroScopePlugIn::CController controller);
    void OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget rt);
private:
    void bind_events();
};