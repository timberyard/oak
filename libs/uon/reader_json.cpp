#include "uon.hpp"

#include <fstream>
#include <sstream>
#include <json_spirit/json_spirit_reader.h>

namespace uon {

Value read_json(std::string input)
{
	std::stringstream stream(input);
	return read_json(stream);
}

Value read_json(std::istream& input)
{
	input.exceptions( std::ifstream::failbit | std::ifstream::badbit );

	json_spirit::Value spval;
	json_spirit::read(input, spval);

	std::function<Value(const json_spirit::Value& spval)> convert = [&convert](const json_spirit::Value& spval) -> Value
	{
		switch(spval.type())
		{
			case json_spirit::obj_type:
			{
				Object obj;
				for(auto& e : spval.get_obj())
				{
					obj[e.name_] = convert(e.value_);
				}
				return obj;
			}

			case json_spirit::array_type:
			{
				Array arr;
				for(auto& e : spval.get_array())
				{
					arr.push_back(convert(e));
				}
				return arr;
			}

			case json_spirit::str_type:
			{
				return spval.get_str();
			}

			case json_spirit::bool_type:
			{
				return spval.get_bool();
			}

			case json_spirit::int_type:
			{
				if(spval.is_uint64()) {
					return static_cast<long double>(spval.get_uint64());
				}
				return static_cast<long double>(spval.get_int64());
			}

			case json_spirit::real_type:
			{
				return static_cast<long double>(spval.get_real());
			}

			case json_spirit::null_type:
			{
				return Null();
			}
		}

		throw std::runtime_error("invalid type");
	};

	return convert(spval);
}

Value read_json(boost::filesystem::path input)
{
	std::ifstream stream;
	stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	stream.open( input.string() );

	return read_json(stream);
}

}
