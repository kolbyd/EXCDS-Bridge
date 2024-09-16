#include "ExcdsFlightPlanData.h"

sio::message::ptr ExcdsFlightPlanData::GenerateFlightPlanDataResponse()
{
    sio::message::ptr response = sio::object_message::create();

    response->get_map()["aircraft"] = GenerateAircraftObject();
    response->get_map()["altitude"] = GenerateAltitudeObject();
    response->get_map()["controllerData"] = GenerateControllerDataObject();
    response->get_map()["fpdata"] = GenerateFlightPlanDataObject();
    response->get_map()["route"] = GenerateRouteObject();
    response->get_map()["speeds"] = GenerateSpeedsObject();
    response->get_map()["times"] = GenerateTimesObject();
    response->get_map()["estimates"] = GenerateEstimatesObject();

    GeneralHelpers::AddTimestamp(response);

    return response;
}

sio::message::ptr ExcdsFlightPlanData::GenerateAircraftObject()
{
    sio::message::ptr response = sio::object_message::create();

    return response;
}

