#pragma once

#include <string>

enum ExcdsResponseType {
    SUCCESS,
    FP_INVALID,
    CANT_GET_DATA,
    AC_NOT_FOUND,
    NO_FPLN,
    ALRDY_TRAKD
};

class ExcdsResponse
{
public:
    ExcdsResponse(ExcdsResponseType type) : _type(type) {};
    std::string toErrorCode();
    std::string toErrorMessage();
private:
    ExcdsResponseType _type;
};
