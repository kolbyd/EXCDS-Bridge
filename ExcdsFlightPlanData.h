#pragma once

#include <string>
#include <sio_message.h>
#include "EuroScopePlugIn.h"
#include "ExcdsResponse.h"
#include "GeneralHelpers.h"

class ExcdsFlightPlanData
{
public:
    ExcdsFlightPlanData(EuroScopePlugIn::CFlightPlan fp) :_flightPlan(fp) {};
    sio::message::ptr GenerateFlightPlanDataResponse();


private:
    EuroScopePlugIn::CFlightPlan _flightPlan;

    sio::message::ptr GenerateAircraftObject();
    sio::message::ptr GenerateAltitudeObject();
    sio::message::ptr GenerateControllerDataObject();
    sio::message::ptr GenerateFlightPlanDataObject();
    sio::message::ptr GenerateRouteObject();
    sio::message::ptr GenerateSpeedsObject();
    sio::message::ptr GenerateTimesObject();
    sio::message::ptr GenerateEstimatesObject();
};