#include "sio_client.h"
#include <string>

class BaseMessage
{
public:
    BaseMessage(std::string);
    sio::message::ptr ToSioMessage();

private:
    std::string _eventName;
};