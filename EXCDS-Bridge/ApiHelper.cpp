#include <string>
#include <iostream>

#include "ApiHelper.h"

struct Authorization
{
	std::string access_token;
	std::string token_type;
	int expires_in;
};

struct Response
{
	std::string message;
	std::string url;
	Authorization authorization;
};

void ApiHelper::Login(std::string callsign, int cid)
{

}
