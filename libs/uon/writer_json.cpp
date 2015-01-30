#include "uon.hpp"

#include <fstream>
#include <sstream>
#include <json_spirit/json_spirit_writer.h>

namespace uon {

std::string write_json(const Value& value, bool compact)
{
	std::stringstream stream;
	write_json(value, stream, compact);
	return stream.str();
}

void write_json(const Value& value, std::ostream& output, bool compact)
{
	output.exceptions( std::ofstream::failbit | std::ofstream::badbit );

	std::function<json_spirit::Value(const Value& val)> convert = [&convert](const Value& val) -> json_spirit::Value
	{
		switch(val.type())
		{
			case Type::null:
			{
				return json_spirit::Value();
			}

			case Type::string:
			{
				return json_spirit::Value(val.as_string());
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
					spobj.push_back(json_spirit::Pair(e.first, convert(e.second)));
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

	json_spirit::write(spval, output, compact ? json_spirit::none : (json_spirit::pretty_print | json_spirit::single_line_arrays));
}

void write_json(const Value& value, boost::filesystem::path output, bool compact)
{
	std::ofstream stream;
	stream.exceptions( std::ofstream::failbit | std::ofstream::badbit );
	stream.open( output.string() );

	return write_json(value, stream);
}

}
