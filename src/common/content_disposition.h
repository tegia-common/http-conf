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

inline constexpr size_t k_max_raw_filename_bytes = 4096;
inline constexpr size_t k_max_sanitized_filename_bytes = 255;

inline bool is_ascii_control(unsigned char ch)
{
	return ch <= 0x1F || ch == 0x7F;
}

inline bool decode_utf8_codepoint(const std::string &value, size_t pos, uint32_t &codepoint, size_t &length)
{
	if(pos >= value.size())
	{
		return false;
	}

	unsigned char b0 = static_cast<unsigned char>(value[pos]);
	if((b0 & 0x80) == 0x00)
	{
		codepoint = b0;
		length = 1;
		return true;
	}

	if((b0 & 0xE0) == 0xC0)
	{
		length = 2;
		if(pos + length > value.size())
		{
			return false;
		}

		unsigned char b1 = static_cast<unsigned char>(value[pos + 1]);
		if((b1 & 0xC0) != 0x80)
		{
			return false;
		}

		codepoint = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
		return codepoint >= 0x80;
	}

	if((b0 & 0xF0) == 0xE0)
	{
		length = 3;
		if(pos + length > value.size())
		{
			return false;
		}

		unsigned char b1 = static_cast<unsigned char>(value[pos + 1]);
		unsigned char b2 = static_cast<unsigned char>(value[pos + 2]);
		if((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
		{
			return false;
		}

		codepoint = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
		return codepoint >= 0x800;
	}

	if((b0 & 0xF8) == 0xF0)
	{
		length = 4;
		if(pos + length > value.size())
		{
			return false;
		}

		unsigned char b1 = static_cast<unsigned char>(value[pos + 1]);
		unsigned char b2 = static_cast<unsigned char>(value[pos + 2]);
		unsigned char b3 = static_cast<unsigned char>(value[pos + 3]);
		if((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
		{
			return false;
		}

		codepoint = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
		return codepoint >= 0x10000 && codepoint <= 0x10FFFF;
	}

	return false;
}

inline bool is_bidi_control_codepoint(uint32_t codepoint)
{
	return
		(codepoint >= 0x202A && codepoint <= 0x202E) ||
		(codepoint >= 0x2066 && codepoint <= 0x2069);
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
	std::string bounded_raw = truncate_utf8_to_bytes(raw, k_max_raw_filename_bytes);
	std::string sanitized;
	sanitized.reserve(std::min(bounded_raw.size(), k_max_sanitized_filename_bytes));

	size_t i = 0;
	while(i < bounded_raw.size())
	{
		unsigned char ch = static_cast<unsigned char>(bounded_raw[i]);
		if((ch & 0x80) == 0x00)
		{
			if(ch == '\r' || ch == '\n' || ch == '\0')
			{
				++i;
				continue;
			}

			if(ch == '/' || ch == '\\')
			{
				sanitized.push_back('_');
				++i;
				continue;
			}

			if(is_ascii_control(ch))
			{
				++i;
				continue;
			}

			sanitized.push_back(static_cast<char>(ch));
			++i;
			continue;
		}

		uint32_t codepoint = 0;
		size_t len = 0;
		if(!decode_utf8_codepoint(bounded_raw, i, codepoint, len))
		{
			++i;
			continue;
		}

		if(is_bidi_control_codepoint(codepoint))
		{
			i += len;
			continue;
		}

		sanitized.append(bounded_raw, i, len);
		i += len;
	}

	sanitized = truncate_utf8_to_bytes(sanitized, k_max_sanitized_filename_bytes);
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
