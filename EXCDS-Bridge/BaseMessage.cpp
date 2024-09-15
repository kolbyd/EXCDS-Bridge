#include "BaseMessage.h"

BaseMessage::BaseMessage(std::string e):_eventName(e) { }

sio::message::ptr BaseMessage::ToSioMessage()
{
	sio::message::ptr msg = sio::object_message::create();

	return msg;
}