#include "uon.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <json_spirit/json_spirit_writer.h>
#include <boost/regex.hpp>

namespace uon {

/*
static std::string utf8_invalid_characters =
"("
    "[\\xC0-\\xC1]" // Invalid UTF-8 Bytes
    "|" "[\\xF5-\\xFF]" // Invalid UTF-8 Bytes
    "|" "\\xE0[\\x80-\\x9F]" // Overlong encoding of prior code point
    "|" "\\xF0[\\x80-\\x8F]" // Overlong encoding of prior code point
    "|" "[\\xC2-\\xDF](?![\\x80-\\xBF])" // Invalid UTF-8 Sequence Start
    "|" "[\\xE0-\\xEF](?![\\x80-\\xBF]{2})" // Invalid UTF-8 Sequence Start
    "|" "[\\xF0-\\xF4](?![\\x80-\\xBF]{3})" // Invalid UTF-8 Sequence Start
    "|" "(?<=[\\x0-\\x7F\\xF5-\\xFF])[\\x80-\\xBF]" // Invalid UTF-8 Sequence Middle
//    "|" "(?<![\\xC2-\\xDF]|[\\xE0-\\xEF]|[\\xE0-\\xEF][\\x80-\\xBF]|[\\xF0-\\xF4]|[\\xF0-\\xF4][\\x80-\\xBF]|[\\xF0-\\xF4][\\x80-\\xBF]{2})[\\x80-\\xBF]" // Overlong Sequence
    "|" "(?<=[\\xE0-\\xEF])[\\x80-\\xBF](?![\\x80-\\xBF])" // Short 3 byte sequence
    "|" "(?<=[\\xF0-\\xF4])[\\x80-\\xBF](?![\\x80-\\xBF]{2})" // Short 4 byte sequence
    "|" "(?<=[\\xF0-\\xF4][\\x80-\\xBF])[\\x80-\\xBF](?![\\x80-\\xBF])" // Short 4 byte sequence (2)
")";

static std::string utf8_replacement_character = "\uFFFD";
*/

std::string hex_byte(unsigned char value) {
	std::stringstream str;
	str << std::noshowbase << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)value << std::flush;
	return str.str();
}

std::string utf8_remove_illegal_chars(std::string value)
{
	std::string result;
	result.reserve(value.length()+1);

	for(std::size_t i = 0, j = value.length(); i < j;)
	{
		if( (value[i] & 0x80) < 0x80 )
		{
			if( value[i] <= 0x1F || value[i] == 0x7F )
			{
		        switch( value[i] )
		        {
		            case '\b':
		            case '\f':
		            case '\n':
		            case '\r':
		            case '\t':
						result += value.substr(i, 1);
						break;
		        }
			}
			else
			{
				result += value.substr(i, 1);
			}
			i += 1;
		}
		else
		if( (value[i] & 0xE0) == 0xC0 && (j-i-1) >= 1 && (value[i+1] & 0xC0) == 0x80 )
		{
			result += value.substr(i, 2);
			i += 2;
		}
		else
		if( (value[i] & 0xF0) == 0xE0 && (j-i-1) >= 2 && (value[i+1] & 0xC0) == 0x80 && (value[i+2] & 0xC0) == 0x80 )
		{
			result += value.substr(i, 3);
			i += 3;
		}
		else
		if( (value[i] & 0xF8) == 0xF0 && (j-i-1) >= 3 && (value[i+1] & 0xC0) == 0x80 && (value[i+2] & 0xC0) == 0x80 && (value[i+3] & 0xC0) == 0x80 )
		{
			result += value.substr(i, 4);
			i += 4;
		}
		else
		{
			while( (value[i] & 0x80) == 0x80 )
			{
				i += 1;
			}
		}
	}

	return result;
}

std::string write_json(const Value& value, bool compact)
{
	std::stringstream stream;
	write_json(value, stream, compact);
	return stream.str();
}

void write_json(const Value& value, std::ostream& output, bool compact)
{
	//boost::regex utf8_illegal_characters_regex(utf8_invalid_characters, boost::regex::perl);

	output.exceptions( std::ofstream::failbit | std::ofstream::badbit );

	std::function<json_spirit::Value(const Value& val)> convert = [&convert /*,&utf8_illegal_characters_regex*/](const Value& val) -> json_spirit::Value
	{
		switch(val.type())
		{
			case Type::null:
			{
				return json_spirit::Value();
			}

			case Type::string:
			{
				return json_spirit::Value(
					utf8_remove_illegal_chars(val.as_string())
					/*boost::regex_replace(val.as_string(), utf8_illegal_characters_regex, utf8_replacement_character, boost::match_default)*/);
			}

			case Type::number:
			{
				auto num = val.as_number();

				long double i = 0.0;
				long double f = std::modf(num, &i);

				if(f == 0.0)
				{
					if(f >= 0)
					{
						return json_spirit::Value(static_cast<std::uint64_t>(i));
					}

					return json_spirit::Value(static_cast<std::int64_t>(i));
				}

				return json_spirit::Value(static_cast<double>(num));
			}

			case Type::boolean:
			{
				return json_spirit::Value(val.as_boolean());
			}

			case Type::object:
			{
				json_spirit::Object spobj;
				for(auto& e : val.as_object())
				{
					spobj.push_back(json_spirit::Pair(
						//boost::regex_replace(e.first, utf8_illegal_characters_regex, utf8_replacement_character, boost::match_default),
						utf8_remove_illegal_chars(e.first),
						convert(e.second)));
				}
				return json_spirit::Value(spobj);
			}

			case Type::array:
			{
				json_spirit::Array sparr;
				for(auto& e : val.as_array())
				{
					sparr.push_back(convert(e));
				}
				return json_spirit::Value(sparr);
			}

		}

		throw std::runtime_error("invalid type");
	};

	auto spval = convert(value);

	json_spirit::write(spval, output, json_spirit::raw_utf8 | (compact ? json_spirit::none : (json_spirit::pretty_print | json_spirit::single_line_arrays)));
}

void write_json(const Value& value, boost::filesystem::path output, bool compact)
{
	std::ofstream stream;
	stream.exceptions( std::ofstream::failbit | std::ofstream::badbit );
	stream.open( output.string() );

	return write_json(value, stream);
}

}
