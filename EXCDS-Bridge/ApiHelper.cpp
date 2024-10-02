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

std::string ApiHelper::ToASCII(const std::string& input) {
    std::string asciiString;

    for (char c : input) {
        if (static_cast<unsigned char>(c) <= 0x7F) {
            asciiString += c;
        }
        else {
            // Optionally, replace non-ASCII characters with a placeholder
            asciiString += ' ';
        }
    }
    return asciiString;
}

