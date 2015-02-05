#include "uon.hpp"

#include <fstream>

#if !defined(_WIN32)
#include <mongo/client/dbclient.h>
#endif

namespace uon {

#if !defined(_WIN32)

void write_bson(const Value& value, std::ostream& input)
{
	input.exceptions( std::ofstream::failbit | std::ofstream::badbit );
	throw std::runtime_error("not implemented");
}

void write_bson(const Value& value, boost::filesystem::path input)
{
	std::ofstream stream;
	stream.exceptions( std::ofstream::failbit | std::ofstream::badbit );
	stream.open( input.string() );

	return write_bson(value, stream);
}

mongo::BSONObj to_mongo_bson(const Value& value)
{
	return mongo::fromjson(write_json(value));
}

#endif

}
