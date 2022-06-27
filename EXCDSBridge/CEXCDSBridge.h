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

private:
    void bind_events();
};
