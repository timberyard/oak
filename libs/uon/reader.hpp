#pragma once

#include <istream>
#include <boost/filesystem.hpp>

namespace uon {

	extern Value read_json(std::istream& input);
	extern Value read_json(boost::filesystem::path input);
	extern Value read_json(std::string input);

	extern Value read_bson(std::istream& input);
	extern Value read_bson(boost::filesystem::path input);
}
