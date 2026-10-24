#include <cmath>
#include <string>

#include "ApiHelper.h"
#include "sio_message.h"

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

namespace {
	std::string SanitizeBytes(const char* src)
	{
		if (!src) {
			return "";
		}

		std::string asciiString;
		for (const char* p = src; *p; ++p) {
			const unsigned char c = static_cast<unsigned char>(*p);
			if (c >= 0x20 && c <= 0x7E) {
				asciiString += static_cast<char>(c);
			}
			else {
				asciiString += ' ';
			}
		}
		return asciiString;
	}
}

void ApiHelper::Login(std::string callsign, int cid)
{

}

std::string ApiHelper::ToASCII(const std::string& input)
{
	return SanitizeBytes(input.c_str());
}

std::string ApiHelper::SafeString(const char* src)
{
	return SanitizeBytes(src);
}

double ApiHelper::SafeDouble(double v)
{
	return std::isfinite(v) ? v : 0.0;
}

sio::message::ptr ApiHelper::SafeStringMessage(const char* src)
{
	return sio::string_message::create(ApiHelper::SafeString(src));
}

sio::message::ptr ApiHelper::SafeStringMessage(const std::string& src)
{
	return sio::string_message::create(ApiHelper::ToASCII(src));
}

std::string ApiHelper::PipeField(const std::string& src, size_t index)
{
	size_t start = 0;
	size_t field = 0;
	while (true) {
		const size_t pipe = src.find('|', start);
		if (field == index) {
			if (pipe == std::string::npos) {
				return src.substr(start);
			}
			return src.substr(start, pipe - start);
		}
		if (pipe == std::string::npos) {
			return "";
		}
		start = pipe + 1;
		++field;
	}
}
