#pragma once
#include "ExcdsEvent.h"

class AltitudeUpdateEvent :
    public ExcdsEvent
{
public:
    void ExecuteEvent(sio::event&, EuroScopePlugIn::CFlightPlan) override;
};

