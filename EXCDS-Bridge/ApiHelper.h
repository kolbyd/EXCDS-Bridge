#pragma once
#include <string>

#include "sio_message.h"

class ApiHelper
{
public:
	static void Login(std::string callsign, int cid);
	static std::string ToASCII(const std::string&);
	static std::string SafeString(const char* src);
	static double SafeDouble(double v);
	static sio::message::ptr SafeStringMessage(const char* src);
	static sio::message::ptr SafeStringMessage(const std::string& src);
	/** Zero-based pipe field from `a|b|c` (empty if missing). */
	static std::string PipeField(const std::string& src, size_t index);
};
