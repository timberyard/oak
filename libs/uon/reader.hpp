#pragma once

#include <istream>
#include <boost/filesystem.hpp>

#if !defined(_WIN32)
#include <mongo/bson/bson.h>
#endif

namespace uon {

	extern Value read_json(std::istream& input);
	extern Value read_json(boost::filesystem::path input);
	extern Value read_json(std::string input);

#if !defined(_WIN32)
	extern Value read_bson(std::istream& input);
	extern Value read_bson(boost::filesystem::path input);

	extern Value from_mongo_bson(const mongo::BSONObj& mval);
#endif
}
