#pragma once
#include "ExcdsEvent.h"
class ScratchpadUpdateEvent :
    public ExcdsEvent
{
public:
    void ExecuteEvent(sio::event&, EuroScopePlugIn::CFlightPlan) override;
};

