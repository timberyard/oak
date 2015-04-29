#include <numeric>
#include <fstream>
#include <set>
#include <iostream>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "tasks.hpp"
#include "process.hpp"
#include "task_utils.hpp"

namespace tasks {

TaskResult task_build_cmake             ( uon::Value config );
TaskResult task_test_googletest         ( uon::Value config );
TaskResult task_analysis_cppcheck       ( uon::Value config );
TaskResult task_doc_doxygen             ( uon::Value config );

std::map<std::string, std::function<TaskResult(uon::Value)>> taskTypes =
{
	{ "build:cmake",             task_build_cmake },
	{ "test:googletest",         task_test_googletest },
	{ "analysis:cppcheck",       task_analysis_cppcheck },
	{ "doc:doxygen",             task_doc_doxygen }
};

bool copyDir(
    boost::filesystem::path const & source,
    boost::filesystem::path const & destination
)
{
    namespace fs = boost::filesystem;
    try
    {
        // Check whether the function call is valid
        if(
            !fs::exists(source) ||
            !fs::is_directory(source)
        )
        {
            std::cerr << "Source directory " << source.string()
                << " does not exist or is not a directory." << '\n'
            ;
            return false;
        }
        if(fs::exists(destination))
        {
            std::cerr << "Destination directory " << destination.string()
                << " already exists." << '\n'
            ;
            return false;
        }
        // Create the destination directory
        if(!fs::create_directory(destination))
        {
            std::cerr << "Unable to create destination directory"
                << destination.string() << '\n'
            ;
            return false;
        }
    }
    catch(fs::filesystem_error const & e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
    // Iterate through the source directory
    for(
        fs::directory_iterator file(source);
        file != fs::directory_iterator(); ++file
    )
    {
        try
        {
            fs::path current(file->path());
            if(fs::is_directory(current))
            {
                // Found directory: Recursion
                if(
                    !copyDir(
                        current,
                        destination / current.filename()
                    )
                )
                {
                    return false;
                }
            }
            else
            {
                // Found file: Copy
                fs::copy_file(
                    current,
                    destination / current.filename()
                );
            }
        }
        catch(fs::filesystem_error const & e)
        {
            std:: cerr << e.what() << '\n';
        }
    }
    return true;
}

TaskResult task_build_cmake( uon::Value config )
{
	// create directories
	boost::filesystem::create_directories(config.get("build.output").to_string());
	boost::filesystem::create_directories(config.get("install.output").to_string());

	// run cmake
	TaskResult result;

	std::vector<std::string> cmakeParams {
		std::string("-DCMAKE_C_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.host.c.binary").to_string(), "\\", "/"),
		std::string("-DCMAKE_CXX_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.host.c++.binary").to_string(), "\\", "/"),
		std::string("-DCMAKE_INSTALL_PREFIX:STRING=") + boost::algorithm::replace_all_copy(config.get("install.output").to_string(), "\\", "/"),

		std::string("-DARCH_HOST_COMPILER_C:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.host.c.binary").to_string(), "\\", "/"),
		std::string("-DARCH_HOST_COMPILER_CXX:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.host.c++.binary").to_string(), "\\", "/"),
		std::string("-DARCH_HOST_OS:STRING=") + config.get("arch.host.os").to_string(),
		std::string("-DARCH_HOST_DISTRIBUTION:STRING=") + config.get("arch.host.distribution").to_string(),
		std::string("-DARCH_HOST_FAMILY:STRING=") + config.get("arch.host.family").to_string(),
		std::string("-DARCH_HOST_BITNESS:STRING=") + config.get("arch.host.bitness").to_string(),
		std::string("-DARCH_HOST_MISC:STRING=") + config.get("arch.host.misc").to_string(),
		std::string("-DARCH_HOST_DESCRIPTOR:STRING=") + config.get("arch.host.descriptor").to_string(),

		std::string("-DARCH_BUILD_COMPILER_C:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.build.c.binary").to_string(), "\\", "/"),
		std::string("-DARCH_BUILD_COMPILER_CXX:STRING=") + boost::algorithm::replace_all_copy(config.get("arch.build.c++.binary").to_string(), "\\", "/"),
		std::string("-DARCH_BUILD_OS:STRING=") + config.get("arch.build.os").to_string(),
		std::string("-DARCH_BUILD_DISTRIBUTION:STRING=") + config.get("arch.build.distribution").to_string(),
		std::string("-DARCH_BUILD_FAMILY:STRING=") + config.get("arch.build.family").to_string(),
		std::string("-DARCH_BUILD_BITNESS:STRING=") + config.get("arch.build.bitness").to_string(),
		std::string("-DARCH_BUILD_MISC:STRING=") + config.get("arch.build.misc").to_string(),
		std::string("-DARCH_BUILD_DESCRIPTOR:STRING=") + config.get("arch.build.descriptor").to_string()
	};

	if(config.get("verbose").to_boolean())
		{ cmakeParams.push_back(std::string("-DCMAKE_VERBOSE_MAKEFILE:BOOLEAN=ON")); }

	for(auto variable : config.get("cmake.variables").as_object())
	{
		cmakeParams.push_back( std::string("-D") + variable.first + std::string(":")
			+ variable.second.get("type").to_string() + std::string("=")
			+ variable.second.get("value").to_string() );
	}

	cmakeParams.push_back(boost::algorithm::replace_all_copy(config.get("source.input").to_string(), "\\", "/"));

#ifdef _WIN32
	cmakeParams.push_back("-G");
	cmakeParams.push_back(config.get("cmake.generator").to_string());
#endif

	process::TextProcessResult cmakeResult = process::executeTextProcess(
		config.get("cmake.binary").to_string(),
		cmakeParams,
		config.get("build.output").to_string());

	result.output.set("cmake", task_utils::createTaskOutput(
		config.get("cmake.binary").to_string(),
		cmakeParams,
		config.get("build.output").to_string(),
		cmakeResult));

	result.message = task_utils::createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	// run make
	if(cmakeResult.exitCode == 0)
	{
		std::vector<std::string> makeParams;

		for(auto variable : config.get("make.variables").as_object())
		{
			makeParams.push_back( variable.first.to_string() + std::string("=") + variable.second.to_string() );
		}

		process::TextProcessResult makeResult = process::executeTextProcess(
			config.get("make.binary").to_string(),
			makeParams,
			config.get("build.output").to_string());

		uon::Array details;

		auto basePath = boost::algorithm::replace_all_copy(config.get("source.base").to_string(), "\\", "/");

		for(auto line : makeResult.output)
		{
			auto i_filename = line.second.find(":");
			auto i_row = line.second.find(":", i_filename+1);
			auto i_column = line.second.find(":", i_row+1);

			auto i_warning = line.second.find("warning:", i_column+1);
			auto i_error = line.second.find("error:", i_column+1);

			if(	   i_filename != std::string::npos
				&& i_row != std::string::npos
				&& i_column != std::string::npos
				&& ( i_warning != std::string::npos || i_error != std::string::npos ) )
			{
				auto filename = line.second.substr(0, i_filename);
				auto row = line.second.substr(i_filename+1, i_row-i_filename-1);
				auto column = line.second.substr(i_row+1, i_column-i_row-1);
				std::string type = (i_error != std::string::npos) ? "error" : "warning";
				auto message = line.second.substr((i_error != std::string::npos) ? i_error+6 : i_warning+8);

				boost::trim(filename);
				boost::trim(row);
				boost::trim(column);
				boost::trim(type);
				boost::trim(message);

				boost::algorithm::replace_all(filename, "\\", "/");

				if(filename.find(basePath) == 0)
				{
					filename = filename.substr(basePath.length());
				}

				boost::trim_left_if(filename, boost::is_any_of("/"));

				uon::Value details_row;
				details_row.set("type", type);
				details_row.set("message", message);
				details_row.set("filename", filename);

                long double row_converted = 0.0;
                long double column_converted = 0.0;
                try
                {
                    row_converted = std::stold(row);
                    column_converted = std::stold(column);
                }
                catch( ... )
                {
                    std::cout << "Could not convert " << row << " or " << column << " to long double" << std::endl;
                }

                details_row.set("row", row_converted);
                details_row.set("column", column_converted);
                
				details.push_back(details_row);
			}
		}

		uon::unique(details);
		result.output.set("results", details);

		result.output.set("make", task_utils::createTaskOutput(
			config.get("make.binary").to_string(),
			makeParams,
			config.get("build.output").to_string(),
			makeResult));

		result.message = task_utils::createTaskMessage(makeResult);

		result.warnings = accumulate(
			details.begin(), details.end(), 0,
			[] (unsigned int count, const uon::Value& el) -> unsigned int {
				return count + (el.get("type") == "warning" ? 1 : 0);
			});

		result.errors = accumulate(
			details.begin(), details.end(), 0,
			[] (unsigned int count, const uon::Value& el) -> unsigned int {
				return count + (el.get("type") == "error" ? 1 : 0);
			});

		result.status =
			((result.errors > 0 || makeResult.exitCode != 0)
				? TaskResult::STATUS_ERROR
				: (result.warnings > 0
					? TaskResult::STATUS_WARNING
					: TaskResult::STATUS_OK));

		// run install
		if(makeResult.exitCode == 0 && config.get("install.enabled").to_boolean())
		{
			std::vector<std::string> installParams{ "install" };

			for(auto variable : config.get("make.variables").as_object())
			{
				installParams.push_back( variable.first + std::string("=") + variable.second.to_string() );
			}

			process::TextProcessResult installResult = process::executeTextProcess(
				config.get("make.binary").to_string(),
				installParams,
				config.get("install.base").to_string());

			result.output.set("install", task_utils::createTaskOutput(
				config.get("make.binary").to_string(),
				installParams,
				config.get("install.base").to_string(),
				installResult));

			result.message = task_utils::createTaskMessage(installResult);
			result.errors += (installResult.exitCode != 0 ? 1 : 0);

			result.status =
				(result.errors
					? TaskResult::STATUS_ERROR
					: (result.warnings > 0
						? TaskResult::STATUS_WARNING
						: TaskResult::STATUS_OK));
		}
	}

	return result;
}


TaskResult task_test_googletest( uon::Value config )
{
	TaskResult result;

	// prepare parameter and dirs
	boost::filesystem::path xmlFilePath = config.get("output").to_string();
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	std::vector<std::string> arguments = {
		"--gtest_output=xml:" + xmlFilePath.string(),
		"--gtest_filter=" + config.get("filter").to_string()
	};

	// run test
	process::TextProcessResult testResult = process::executeTextProcess(config.get("binary").to_string(), arguments, parentPath.string());

	// read XML result file
	boost::property_tree::ptree xmlTestResult;

	std::ifstream xmlTestResultStream;
	xmlTestResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	xmlTestResultStream.open( xmlFilePath.string() );

	boost::property_tree::read_xml( xmlTestResultStream, xmlTestResult, boost::property_tree::xml_parser::trim_whitespace );

	// generate outline table
	uon::Value table_outline;

	{
		uon::Value row;
		row.set("name", xmlTestResult.get<std::string>("testsuites.<xmlattr>.name"));
		row.set("tests", xmlTestResult.get<uon::Number>("testsuites.<xmlattr>.tests"));
		row.set("failures", xmlTestResult.get<uon::Number>("testsuites.<xmlattr>.failures"));
		row.set("errors", xmlTestResult.get<uon::Number>("testsuites.<xmlattr>.errors"));
		row.set("disabled", xmlTestResult.get<uon::Number>("testsuites.<xmlattr>.disabled"));
		row.set("time", xmlTestResult.get<uon::Number>("testsuites.<xmlattr>.time"));
		row.set("result", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? std::string("Error") : std::string("Ok")));

		table_outline.set("all", row);
	}

	uon::Value testsuites;
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		uon::Value row;
		row.set("name", testsuite.second.get<std::string>("<xmlattr>.name"));
		row.set("tests", testsuite.second.get<uon::Number>("<xmlattr>.tests"));
		row.set("failures", testsuite.second.get<uon::Number>("<xmlattr>.failures"));
		row.set("errors", testsuite.second.get<uon::Number>("<xmlattr>.errors"));
		row.set("disabled", testsuite.second.get<uon::Number>("<xmlattr>.disabled"));
		row.set("time", testsuite.second.get<uon::Number>("<xmlattr>.time"));
		row.set("result", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? std::string("Error") : std::string("Ok")));

		testsuites.set( testsuite.second.get<std::string>("<xmlattr>.name"), row );
	}

	table_outline.set("details", testsuites);
	result.output.set("testsuites", table_outline);

	// generate details table
	uon::Value table_details;
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		for ( auto testcase : testsuite.second )
		{
			if(testcase.first == "<xmlattr>")
				continue;

			std::string name = testcase.second.get<std::string>("<xmlattr>.classname") + "." + testcase.second.get<std::string>("<xmlattr>.name");

			uon::Value row;
			row.set("name", name);
			row.set("status", testcase.second.get<std::string>("<xmlattr>.status"));
			row.set("message", (testcase.second.count("failure") > 0 ? testcase.second.get<std::string>("failure.<xmlattr>.message") : std::string("")));
			row.set("time", testcase.second.get<uon::Number>("<xmlattr>.time"));
			row.set("result", (testcase.second.count("failure") > 0 ? std::string("Error") : std::string("Ok")));

			table_details.set(name, row);
		}
	}

	result.output.set("tests", table_details);

	// generate console output
	for(auto line : testResult.output)
	{
		if(line.second.find("[  FAILED  ]") != std::string::npos)
		{
			line.first = process::TextProcessResult::ERROR_LINE;
		}
		else
		if(line.second.find("[       OK ]") != std::string::npos || line.second.find("[  PASSED  ]") != std::string::npos)
		{
			line.first = process::TextProcessResult::OK_LINE;
		}
	}

	result.output.set("googletest", task_utils::createTaskOutput(config.get("binary").to_string(), arguments, parentPath.string(), testResult));

	// generate meta data
	result.message = task_utils::createTaskMessage(testResult);
	result.warnings = 0;
	result.errors = xmlTestResult.get<int>("testsuites.<xmlattr>.failures") + xmlTestResult.get<int>("testsuites.<xmlattr>.errors");
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}


TaskResult task_analysis_cppcheck( uon::Value config )
{
	boost::filesystem::path xmlFilePath = config.get("output").to_string();
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	TaskResult result;

	std::vector<std::string> arguments { "--xml-version=2", "--enable=all", "--suppress=missingIncludeSystem", "--quiet", config.get("source").to_string() };

	process::TextProcessResult checkResult = process::executeTextProcess(
		config.get("binary").to_string(),
		arguments,
		config.get("source").to_string());

	if(checkResult.exitCode == 0)
	{
		std::string xmlCheckData;

		for(auto line = checkResult.output.begin(); line != checkResult.output.end(); )
		{
			if(line->first == process::TextProcessResult::ERROR_LINE)
			{
				xmlCheckData += line->second + std::string("\n");
				line = checkResult.output.erase(line);
			}
			else
			{
				++line;
			}
		}

		// write xml data
		std::ofstream xmlCheckPersistStream;
		xmlCheckPersistStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		xmlCheckPersistStream.open( config.get("output").to_string() );

		xmlCheckPersistStream << xmlCheckData;
		xmlCheckPersistStream.close();

		// read XML result data
		boost::property_tree::ptree xmlCheckResult;

		std::istringstream xmlCheckResultStream(xmlCheckData);
		xmlCheckResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

		boost::property_tree::read_xml( xmlCheckResultStream, xmlCheckResult, boost::property_tree::xml_parser::trim_whitespace );

		// interpret xml data

		auto basePath = boost::algorithm::replace_all_copy(config.get("base").to_string(), "\\", "/");

		uon::Array errors;
		for ( auto error : xmlCheckResult.get_child("results.errors") )
		{
			if(error.first == "<xmlattr>")
				continue;

			auto attr_file = error.second.get_optional<std::string>("location.<xmlattr>.file");
			auto attr_line = error.second.get_optional<std::string>("location.<xmlattr>.line");

			auto file_rel = attr_file ? *attr_file : std::string("");
			boost::algorithm::replace_all(file_rel, "\\", "/");

			if(file_rel.find(basePath) == 0)
			{
				file_rel = file_rel.substr(basePath.length());
			}

			boost::trim_left_if(file_rel, boost::is_any_of("/"));

			uon::Value row;
			row.set("type", error.second.get<std::string>("<xmlattr>.id"));
			row.set("severity", error.second.get<std::string>("<xmlattr>.severity"));
			row.set("message", error.second.get<std::string>("<xmlattr>.msg"));
			row.set("file", file_rel);
			row.set("line", attr_line ? *attr_line : std::string(""));

			errors.push_back(row);
		}
		result.output.set("errors", errors);

		result.warnings = errors.size();
		result.errors = 0;
	}
	else
	{
		result.warnings = 0;
		result.errors = 1;
	}

	result.output.set("cppcheck", task_utils::createTaskOutput(
		config.get("binary").to_string(),
		arguments,
		config.get("source").to_string(),
		checkResult));

	result.message = task_utils::createTaskMessage(checkResult);
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : (result.warnings > 0 ?  TaskResult::STATUS_WARNING : TaskResult::STATUS_OK));

	return result;
}


TaskResult task_doc_doxygen( uon::Value config )
{
	TaskResult result;

	// prepare arguments and dirs

	const std::string sourcePath = config.get("source").to_string();
	const std::string outputPath = config.get("output").to_string();
	const std::string doxyfilePath = (boost::filesystem::path(outputPath) / boost::filesystem::path("doxyfile")).string();

	boost::filesystem::create_directories(outputPath);

	// generate doxyfile

	std::ofstream doxyfileStream;
	doxyfileStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	doxyfileStream.open( doxyfilePath );

	for( auto data : config.get("doxyfile").as_object() )
	{
		doxyfileStream << data.first << " = " << data.second.to_string() << '\n';
	}

	doxyfileStream << "INPUT" << " = " << sourcePath << '\n';
	doxyfileStream << "OUTPUT_DIRECTORY" << " = " << outputPath << '\n';

	doxyfileStream.close();

	// run doxygen
	process::TextProcessResult doxygenResult = process::executeTextProcess(config.get("binary").to_string(), std::vector<std::string>{doxyfilePath}, outputPath);

	result.output.set("doxygen", task_utils::createTaskOutput(config.get("binary").to_string(), std::vector<std::string>{doxyfilePath}, outputPath, doxygenResult));

	result.message = task_utils::createTaskMessage(doxygenResult);
	result.warnings = 0;
	result.errors = (doxygenResult.exitCode != 0 ? 1 : 0);
	result.status = (doxygenResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}

std::string toString(TaskResult::Status status)
{
	switch(status)
	{
		case TaskResult::STATUS_OK : return "ok";
		case TaskResult::STATUS_WARNING : return "warning";
		case TaskResult::STATUS_ERROR : return "error";
	}
	throw std::logic_error("Unsupportet TaskResult::Status!");
}

} // namespace: tasks
