#pragma once

#include <vector>
#include <utility>
#include <string>

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

TextProcessResult executeTextProcess(const std::string& binary, std::vector<std::string> arguments, const std::string& workingDirectory);
