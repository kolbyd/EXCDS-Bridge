#include "ExcdsResponse.h"

std::string ExcdsResponse::toErrorCode()
{
    switch (_type)
    {
    case SUCCESS:
        return "SUCCESS";
    case FP_INVALID:
        return "FP INVALID";
    case CANT_GET_DATA:
        return "CANT_GET_DATA";
    case AC_NOT_FOUND:
        return "AC_NT_FOUND";
    case NO_FPLN:
        return "NO_FPLN";
    case ALRDY_TRAKD:
        return "ALRDY_TRAKD";
    default:
        return "UNKNOWN";
    }
}

std::string ExcdsResponse::toErrorMessage()
{
    switch (_type)
    {
    case SUCCESS:
        return "Success";
    case FP_INVALID:
        return "Flight plan is invalid";
    case CANT_GET_DATA:
        return "Cannot get data";
    case AC_NOT_FOUND:
        return "Aircraft not found";
    case NO_FPLN:
        return "No flight plan found";
    case ALRDY_TRAKD:
        return "Aircraft already tracked";
    default:
        return "Unknown error";
    }
}