#pragma once
#include "sio_client.h"
#include "EuroScopePlugIn.h"

class CEXCDSBridge : public EuroScopePlugIn::CPlugIn
{
public:
    static CEXCDSBridge* GetInstance();
    static sio::socket::ptr GetSocket();
    static void SendEuroscopeMessage(const char* callsign, char* message, char* id);

    CEXCDSBridge();
    virtual ~CEXCDSBridge();

    // void OnAirportRunwayActivityChanged();

    void OnTimer(int Counter);
    void OnRadarTargetPositionUpdate(EuroScopePlugIn::CRadarTarget rt);

    // Listens to EuroScope
    //void OnFlightPlanFlightPlanDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan);
    //void OnFlightPlanControllerAssignedDataUpdate(EuroScopePlugIn::CFlightPlan FlightPlan, int DataType);
    //void OnRefreshFpListContent(EuroScopePlugIn::CFlightPlanList AcList);
    void OnFlightPlanDisconnect(EuroScopePlugIn::CFlightPlan FlightPlan);
private:
    void bind_events();
};