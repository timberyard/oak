#include "uon.hpp"

#include <fstream>

namespace uon {

Value read_bson(std::istream& input)
{
	input.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	throw std::runtime_error("not implemented");
}

Value read_bson(boost::filesystem::path input)
{
	std::ifstream stream;
	stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	stream.open( input.string() );

	return read_bson(stream);
}

Value from_mongo_bson(const mongo::BSONObj& mval)
{
	return read_json(mval.jsonString());
}

}
