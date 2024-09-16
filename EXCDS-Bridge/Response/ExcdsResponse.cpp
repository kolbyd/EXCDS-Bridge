#include "ExcdsResponse.h"

std::string ExcdsResponse::GetCode()
{
    switch (_responseType)
    {
    case ExcdsResponseType::SUCCESS:
        return "SUCCESS";
    case ExcdsResponseType::NO_FPLN:
        return "NO_FPLN";
    case ExcdsResponseType::ALRDY_TRACKD:
        return "ALRDY_TRACKD";
    case ExcdsResponseType::SCRCHPD_STRNG_NOT_SET:
        return "SCRCHPD_STRNG_NOT_SET";
    case ExcdsResponseType::SCRCHPD_NOT_MODIFIED:
        return "SCRCHPD_NOT_MODIFIED";
    default:
        return "UNKNOWN";
    }
}

std::string ExcdsResponse::GetMessage()
{
    switch (_responseType)
    {
    case ExcdsResponseType::SUCCESS:
        return "Success";
    case ExcdsResponseType::NO_FPLN:
        return "Flight plan not found.";
    case ExcdsResponseType::ALRDY_TRACKD:
        return "Aircraft is being tracked by another controller.";
    case ExcdsResponseType::SCRCHPD_STRNG_NOT_SET:
        return "Cannot set scratchpad string.";
    case ExcdsResponseType::SCRCHPD_NOT_MODIFIED:
        return "Scratchpad not modified.";
    default:
        return "Unknown response.";
    }
}