#ifndef H_HTTP_CONTENT_DISPOSITION
#define H_HTTP_CONTENT_DISPOSITION

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace HTTP {
namespace headers {

inline bool is_ascii_control(unsigned char ch)
{
	return ch <= 0x1F || ch == 0x7F;
}

inline std::string truncate_utf8_to_bytes(const std::string &value, size_t max_bytes)
{
	if(value.size() <= max_bytes)
	{
		return value;
	}

	size_t cut = max_bytes;
	while(cut > 0 && (static_cast<unsigned char>(value[cut]) & 0xC0) == 0x80)
	{
		--cut;
	}

	if(cut == 0)
	{
		return "";
	}

	unsigned char lead = static_cast<unsigned char>(value[cut]);
	size_t expected_len = 1;

	if((lead & 0xE0) == 0xC0)
	{
		expected_len = 2;
	}
	else if((lead & 0xF0) == 0xE0)
	{
		expected_len = 3;
	}
	else if((lead & 0xF8) == 0xF0)
	{
		expected_len = 4;
	}

	if(cut + expected_len > max_bytes)
	{
		return value.substr(0, cut);
	}

	return value.substr(0, max_bytes);
}

inline std::string sanitize_download_filename(const std::string &raw)
{
	std::string sanitized;
	sanitized.reserve(raw.size());

	for(unsigned char ch : raw)
	{
		if(ch == '\r' || ch == '\n' || ch == '\0')
		{
			continue;
		}

		if(ch == '/' || ch == '\\')
		{
			sanitized.push_back('_');
			continue;
		}

		if(is_ascii_control(ch))
		{
			continue;
		}

		sanitized.push_back(static_cast<char>(ch));
	}

	sanitized = truncate_utf8_to_bytes(sanitized, 255);
	if(sanitized.empty())
	{
		return "download.bin";
	}

	return sanitized;
}

inline std::string build_ascii_fallback_filename(const std::string &sanitized)
{
	std::string ext;
	size_t dot = sanitized.find_last_of('.');
	if(dot != std::string::npos && dot + 1 < sanitized.size())
	{
		ext = sanitized.substr(dot + 1);
	}

	bool valid = !ext.empty() && ext.size() <= 16;
	for(char ch : ext)
	{
		unsigned char uch = static_cast<unsigned char>(ch);
		if(!std::isalnum(uch))
		{
			valid = false;
			break;
		}
	}

	if(valid)
	{
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
		return "download." + ext;
	}

	return "download.bin";
}

inline bool is_rfc5987_attr_char(unsigned char ch)
{
	if(std::isalnum(ch))
	{
		return true;
	}

	switch(ch)
	{
		case '!':
		case '#':
		case '$':
		case '&':
		case '+':
		case '-':
		case '.':
		case '^':
		case '_':
		case '`':
		case '|':
		case '~':
			return true;
		default:
			return false;
	}
}

inline std::string encode_rfc5987_value(const std::string &utf8)
{
	std::ostringstream out;
	out << std::uppercase << std::hex;

	for(unsigned char ch : utf8)
	{
		if(is_rfc5987_attr_char(ch))
		{
			out << static_cast<char>(ch);
		}
		else
		{
			out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
		}
	}

	return out.str();
}

inline std::string build_content_disposition_attachment(const std::string &raw_filename)
{
	std::string sanitized = sanitize_download_filename(raw_filename);
	std::string fallback = build_ascii_fallback_filename(sanitized);
	std::string encoded = encode_rfc5987_value(sanitized);

	return "attachment; filename=\"" + fallback + "\"; filename*=UTF-8''" + encoded;
}

} // namespace headers
} // namespace HTTP

#endif
