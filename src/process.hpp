#pragma once

#include <vector>
#include <utility>
#include <string>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace process {

struct TextProcessResult
{
	enum LineType
	{
		INFO_LINE,
		ERROR_LINE,
		OK_LINE
	};

	std::vector<std::pair<LineType, std::string>> output;
	int exitCode;

	TextProcessResult() : exitCode(0) { }
	TextProcessResult(const TextProcessResult& o)
		: output(o.output), exitCode(o.exitCode) { }
};

std::string toString(TextProcessResult::LineType lineType);

TextProcessResult executeTextProcess(boost::filesystem::path binary, std::vector<std::string> arguments, boost::filesystem::path workingDirectory);

} // namespace: process
