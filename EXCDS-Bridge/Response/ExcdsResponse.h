#pragma once

#include <string>

enum ExcdsResponseType {
    /**
    * Global response types
    */
    SUCCESS,
    NO_FPLN,
    ALRDY_TRACKD,

    /**
    * AltitudeUpdateEvent response types
    */
    ALT_EXCEPTION,

    /**
    * ScratchpadUpdateEvent response types
    */
    SCRCHPD_STRNG_NOT_SET,
    SCRCHPD_NOT_MODIFIED,

    UNKNOWN
};

class ExcdsResponse
{
public:
    ExcdsResponse(ExcdsResponseType t): _responseType(t) {};

    std::string GetCode();
    std::string GetExcdsMessage();
private:
    ExcdsResponseType _responseType;
};

