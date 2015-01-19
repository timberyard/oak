
#include <numeric>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <json_spirit/json_spirit.h>

#include "tasks.hpp"
#include "process.hpp"
#include "task.utils.hpp"

namespace pt = boost::property_tree;
namespace js = json_spirit;

TaskResult task_build_cmake    ( const pt::ptree& config );
TaskResult task_test_googletest( const pt::ptree& config );
TaskResult task_analysis_cppcheck  ( const pt::ptree& config );
TaskResult task_doc_doxygen    ( const pt::ptree& config );
TaskResult task_publish_rsync  ( const pt::ptree& config );

std::map<std::string, std::function<TaskResult(const pt::ptree&)>> taskTypes =
{
	{ "build:cmake",       task_build_cmake },
	{ "test:googletest",   task_test_googletest },
	{ "analysis:cppcheck", task_analysis_cppcheck },
	{ "doc:doxygen",       task_doc_doxygen },
	{ "publish:rsync",     task_publish_rsync }
};

TaskResult task_build_cmake( const pt::ptree& config )
{
	boost::filesystem::create_directories(config.get<std::string>("build:output"));
	boost::filesystem::create_directories(config.get<std::string>("install:output"));

	TaskResult result;

	std::vector<std::string> cmakeParams {

		std::string("-DCMAKE_C_COMPILER:STRING=") + config.get<std::string>("target.c:binary"),
		std::string("-DCMAKE_CXX_COMPILER:STRING=") + config.get<std::string>("target.c++:binary"),
		std::string("-DTARGET_ARCHITECTURE:STRING=") + config.get<std::string>("target.architecture"),
		std::string("-DTARGET_BITNESS:STRING=") + config.get<std::string>("target.bitness"),
		std::string("-DTARGET_OS:STRING=") + config.get<std::string>("target.os"),

		std::string("-DLOCAL_C_COMPILER:STRING=") + config.get<std::string>("local.c:binary"),
		std::string("-DLOCAL_CXX_COMPILER:STRING=") + config.get<std::string>("local.c++:binary"),
		std::string("-DLOCAL_ARCHITECTURE:STRING=") + config.get<std::string>("local.architecture"),
		std::string("-DLOCAL_BITNESS:STRING=") + config.get<std::string>("local.bitness"),
		std::string("-DLOCAL_OS:STRING=") + config.get<std::string>("local.os"),

		std::string("-DCMAKE_INSTALL_PREFIX=") + config.get<std::string>("install:output"),

		config.get<std::string>("source")
	};

	cmakeParams.insert(cmakeParams.begin(), std::string("-DCMAKE_VERBOSE_MAKEFILE:BOOLEAN=ON"));

#ifdef _WIN32
	cmakeParams.push_back("-G");
	cmakeParams.push_back("MSYS Makefiles");
#endif

	TextProcessResult cmakeResult = executeTextProcess(
		config.get<std::string>("cmake:binary"),
		cmakeParams,
		config.get<std::string>("build:output"));

	result.output.emplace_back("cmake", createTaskOutput(
		config.get<std::string>("cmake:binary"),
		cmakeParams,
		config.get<std::string>("build:output"),
		cmakeResult));

	result.message = createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	if(cmakeResult.exitCode == 0)
	{
		TextProcessResult makeResult = executeTextProcess(
			config.get<std::string>("make:binary"),
			std::vector<std::string>{ },
			config.get<std::string>("build:output"));

		result.output.emplace_back("make", createTaskOutput(
			config.get<std::string>("make:binary"),
			std::vector<std::string>{ },
			config.get<std::string>("build:output"),
			makeResult));

		result.message = createTaskMessage(makeResult);

		result.warnings = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": warning:") != std::string::npos ? 1 : 0);
			});

		result.errors = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": error:") != std::string::npos ? 1 : 0);
			});

		result.status =
			((result.errors > 0 || makeResult.exitCode != 0)
				? TaskResult::STATUS_ERROR
				: (result.warnings > 0
					? TaskResult::STATUS_WARNING
					: TaskResult::STATUS_OK));

		if(makeResult.exitCode == 0)
		{
			TextProcessResult installResult = executeTextProcess(
				config.get<std::string>("make:binary"),
				std::vector<std::string>{ "install" },
				config.get<std::string>("build:output"));

			result.output.emplace_back("install", createTaskOutput(
				config.get<std::string>("make:binary"),
				std::vector<std::string>{ "install" },
				config.get<std::string>("build:output"),
				installResult));

			result.message = createTaskMessage(installResult);
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


TaskResult task_test_googletest( const pt::ptree& config )
{
	TaskResult result;

	// prepare parameter and dirs
	boost::filesystem::path xmlFilePath = config.get<std::string>("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	std::vector<std::string> arguments = {
		"--gtest_output=xml:" + xmlFilePath.string(),
		"--gtest_filter=" + config.get<std::string>("filter")
	};

	// run test
	TextProcessResult testResult = executeTextProcess(config.get<std::string>("binary"), arguments, parentPath.string());

	// read XML result file
	pt::ptree xmlTestResult;

	std::ifstream xmlTestResultStream;
	xmlTestResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	xmlTestResultStream.open( xmlFilePath.string() );

	pt::read_xml( xmlTestResultStream, xmlTestResult, pt::xml_parser::trim_whitespace );

	// generate outline table
	js::Object table_outline;

	{
		js::Object row;
		row.emplace_back("name", xmlTestResult.get<std::string>("testsuites.<xmlattr>.name"));
		row.emplace_back("tests", xmlTestResult.get<std::string>("testsuites.<xmlattr>.tests"));
		row.emplace_back("failures", xmlTestResult.get<std::string>("testsuites.<xmlattr>.failures"));
		row.emplace_back("errors", xmlTestResult.get<std::string>("testsuites.<xmlattr>.errors"));
		row.emplace_back("disabled", xmlTestResult.get<std::string>("testsuites.<xmlattr>.disabled"));
		row.emplace_back("time", xmlTestResult.get<std::string>("testsuites.<xmlattr>.time"));
		row.emplace_back("status", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.emplace_back("all", row);
	}

	js::Object testsuites;
	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		js::Object row;
		row.emplace_back("name", testsuite.second.get<std::string>("<xmlattr>.name"));
		row.emplace_back("tests", testsuite.second.get<std::string>("<xmlattr>.tests"));
		row.emplace_back("failures", testsuite.second.get<std::string>("<xmlattr>.failures"));
		row.emplace_back("errors", testsuite.second.get<std::string>("<xmlattr>.errors"));
		row.emplace_back("disabled", testsuite.second.get<std::string>("<xmlattr>.disabled"));
		row.emplace_back("time", testsuite.second.get<std::string>("<xmlattr>.time"));
		row.emplace_back("status", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		// testsuites.push_back(row); // if we want the task list as array
		testsuites.emplace_back( testsuite.second.get<std::string>("<xmlattr>.name"), row ); // if we want the task list as object with the task name as key
	}
	table_outline.emplace_back("details", testsuites);
	result.output.emplace_back("testsuites", table_outline);

	// generate details table
	js::Object table_details;
	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		for ( auto& testcase : testsuite.second )
		{
			if(testcase.first == "<xmlattr>")
				continue;

			std::string name = testcase.second.get<std::string>("<xmlattr>.classname") + "." + testcase.second.get<std::string>("<xmlattr>.name");

			js::Object row;
			row.emplace_back("name", name);
			row.emplace_back("status", testcase.second.get<std::string>("<xmlattr>.status"));
			row.emplace_back("message", (testcase.second.count("failure") > 0 ? testcase.second.get<std::string>("failure.<xmlattr>.message") : ""));
			row.emplace_back("time", testcase.second.get<std::string>("<xmlattr>.time"));
			row.emplace_back("status", (testcase.second.count("failure") > 0 ? "Error" : "Ok"));

			table_details.emplace_back(name, row);
		}
	}

	result.output.emplace_back("tests", table_details);

	// generate console output
	for(auto& line : testResult.output)
	{
		if(line.second.find("[  FAILED  ]") != std::string::npos)
		{
			line.first = TextProcessResult::ERROR_LINE;
		}
		else
		if(line.second.find("[       OK ]") != std::string::npos || line.second.find("[  PASSED  ]") != std::string::npos)
		{
			line.first = TextProcessResult::OK_LINE;
		}
	}

	result.output.emplace_back("googletest", createTaskOutput(config.get<std::string>("binary"), arguments, parentPath.string(), testResult));

	// generate meta data
	result.message = createTaskMessage(testResult);
	result.warnings = 0;
	result.errors = xmlTestResult.get<int>("testsuites.<xmlattr>.failures") + xmlTestResult.get<int>("testsuites.<xmlattr>.errors");
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}


TaskResult task_analysis_cppcheck( const pt::ptree& config )
{
	boost::filesystem::path xmlFilePath = config.get<std::string>("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	TaskResult result;

	std::vector<std::string> arguments { "--xml-version=2", "--enable=all", "--suppress=missingIncludeSystem", "." };

	TextProcessResult checkResult = executeTextProcess(
		config.get<std::string>("binary"),
		arguments,
		config.get<std::string>("source"));

	if(checkResult.exitCode == 0)
	{
		std::string xmlCheckData;

		for(auto line = checkResult.output.begin(); line != checkResult.output.end(); )
		{
			if(line->first == TextProcessResult::ERROR_LINE)
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
		xmlCheckPersistStream.open( config.get<std::string>("output") );

		xmlCheckPersistStream << xmlCheckData;
		xmlCheckPersistStream.close();

		// read XML result data
		pt::ptree xmlCheckResult;

		std::istringstream xmlCheckResultStream(xmlCheckData);
		xmlCheckResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

		pt::read_xml( xmlCheckResultStream, xmlCheckResult, pt::xml_parser::trim_whitespace );

		// interpret xml data
		js::Array errors;
		for ( auto& error : xmlCheckResult.get_child("results.errors") )
		{
			if(error.first == "<xmlattr>")
				continue;

			js::Object row;
			row.emplace_back("type", error.second.get<std::string>("<xmlattr>.id"));
			row.emplace_back("severity", error.second.get<std::string>("<xmlattr>.severity"));
			row.emplace_back("message", error.second.get<std::string>("<xmlattr>.msg"));

			errors.push_back(row);
		}
		result.output.emplace_back("errors", errors);

		result.warnings = errors.size();
		result.errors = 0;
	}
	else
	{
		result.warnings = 0;
		result.errors = 1;
	}

	result.output.emplace_back("cppcheck", createTaskOutput(
		config.get<std::string>("binary"),
		arguments,
		config.get<std::string>("source"),
		checkResult));

	result.message = createTaskMessage(checkResult);
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : (result.warnings > 0 ?  TaskResult::STATUS_WARNING : TaskResult::STATUS_OK));

	return result;
}


TaskResult task_doc_doxygen( const pt::ptree& config )
{
	TaskResult result;

	// prepare arguments and dirs

	const std::string sourcePath = config.get<std::string>("source");
	const std::string outputPath = config.get<std::string>("output");
	const std::string doxyfilePath = (boost::filesystem::path(outputPath) / boost::filesystem::path("doxyfile")).string();

	boost::filesystem::create_directories(outputPath);

	// generate doxyfile

	std::ofstream doxyfileStream;
	doxyfileStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	doxyfileStream.open( doxyfilePath );

	for( auto& data : config.get_child("doxyfile") )
	{
		doxyfileStream << data.first << " = " << data.second.data() << '\n';
	}

	doxyfileStream << "INPUT" << " = " << sourcePath << '\n';
	doxyfileStream << "OUTPUT_DIRECTORY" << " = " << outputPath << '\n';

	doxyfileStream.close();

	// run doxygen
	TextProcessResult doxygenResult = executeTextProcess(config.get<std::string>("binary"), std::vector<std::string>{doxyfilePath}, outputPath);

	// generate task output
	result.output.emplace_back("doxygen", createTaskOutput(config.get<std::string>("binary"), std::vector<std::string>{doxyfilePath}, outputPath, doxygenResult));

	// generate meta data
	result.message = createTaskMessage(doxygenResult);
	result.warnings = 0;
	result.errors = (doxygenResult.exitCode != 0 ? 1 : 0);
	result.status = (doxygenResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}

TaskResult task_publish_rsync( const pt::ptree& config )
{
	TaskResult result;

	// run ssh:mkdir

	std::vector<std::string> sshArgs {
		"-o", "BatchMode yes", "-p", config.get<std::string>("destination.port"),
		config.get<std::string>("destination.user") + std::string("@") + config.get<std::string>("destination.host"),
		"mkdir", "-p",
		config.get<std::string>("destination.base") + std::string("/") + config.get<std::string>("destination.directory")
	};

	TextProcessResult sshResult = executeTextProcess(config.get<std::string>("ssh:binary"), sshArgs, config.get<std::string>("source"));

	result.output.emplace_back("ssh:mkdir", createTaskOutput(config.get<std::string>("ssh:binary"), sshArgs, config.get<std::string>("source"), sshResult));

	if(sshResult.exitCode != 0)
	{
		result.message = createTaskMessage(sshResult);
		result.warnings = 0;
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;

		return result;
	}

	// run rsync
	std::vector<std::string> rsyncArgs {
		std::string("--rsh=") + config.get<std::string>("ssh:binary") + (" -o \"BatchMode yes\" -p ") + config.get<std::string>("destination.port"),
		"--archive", "--delete", "--verbose",
		config.get<std::string>("source") + std::string("/"),
		config.get<std::string>("destination.user") + std::string("@") + config.get<std::string>("destination.host") + std::string(":")
		+ config.get<std::string>("destination.base") + std::string("/") + config.get<std::string>("destination.directory") + std::string("/")
	};

	TextProcessResult rsyncResult = executeTextProcess(config.get<std::string>("rsync:binary"), rsyncArgs, config.get<std::string>("source"));

	result.output.emplace_back("rsync", createTaskOutput(config.get<std::string>("rsync:binary"), rsyncArgs, config.get<std::string>("source"), rsyncResult));

	// generate meta data
	result.message = createTaskMessage(rsyncResult);
	result.warnings = 0;
	result.errors = (rsyncResult.exitCode != 0 ? 1 : 0);
	result.status = (rsyncResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

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
