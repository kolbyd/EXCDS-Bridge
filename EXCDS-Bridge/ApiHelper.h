#pragma once
#include <string>

namespace sio {
	class message;
}

class ApiHelper
{
public:
	static void Login(std::string callsign, int cid);
	static std::string ToASCII(const std::string&);
	static std::string SafeString(const char* src);
	static double SafeDouble(double v);
	static sio::message::ptr SafeStringMessage(const char* src);
	static sio::message::ptr SafeStringMessage(const std::string& src);
};
