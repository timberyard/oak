#pragma once

#include <ostream>
#include <boost/filesystem.hpp>
#include <mongo/bson/bson.h>

namespace uon {

	extern void write_json(const Value& value, std::ostream& output, bool compact = false);
	extern void write_json(const Value& value, boost::filesystem::path output, bool compact = false);
	extern std::string write_json(const Value& value, bool compact = false);

	extern void write_bson(const Value& value, std::ostream& output);
	extern void write_bson(const Value& value, boost::filesystem::path output);

	extern mongo::BSONObj to_mongo_bson(const Value& value);
}
