#pragma once
#include <string>

class ApiHelper
{
public:
	static void Login(std::string callsign, int cid);
	static std::string ToASCII(const std::string&);
};