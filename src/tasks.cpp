
#include <numeric>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <json_spirit/json_spirit.h>

#include "tasks.hpp"
#include "process.hpp"
#include "task.utils.hpp"

using namespace std;
using namespace boost::property_tree;

namespace js = json_spirit;

TaskResult task_build_cmake    ( const ptree& config );
TaskResult task_test_googletest( const ptree& config );
TaskResult task_test_cppcheck  ( const ptree& config );
TaskResult task_doc_doxygen    ( const ptree& config );

map<string, function<TaskResult(const ptree&)>> taskTypes =
{
	{ "build:cmake",     task_build_cmake },
	{ "test:googletest", task_test_googletest },
	{ "test:cppcheck",   task_test_cppcheck },
	{ "doc:doxygen",     task_doc_doxygen }
};

TaskResult task_build_cmake( const ptree& config )
{
	boost::filesystem::remove_all(config.get<string>("output"));
	boost::filesystem::create_directories(config.get<string>("output"));

	TaskResult result;

	vector<string> cmakeParams { config.get<string>("source") };

	cmakeParams.insert(cmakeParams.begin(), std::string("-DCMAKE_C_COMPILER:STRING=") + config.get<string>("c:binary"));
	cmakeParams.insert(cmakeParams.begin(), std::string("-DCMAKE_CXX_COMPILER:STRING=") + config.get<string>("c++:binary"));

#ifdef _WIN32
	cmakeParams.push_back("-G");
	cmakeParams.push_back("MSYS Makefiles");
#endif

	TextProcessResult cmakeResult = executeTextProcess(
		config.get<string>("cmake:binary"),
		cmakeParams,
		config.get<string>("output"));

	result.output.emplace_back("cmake", createTaskOutput(
		config.get<string>("cmake:binary"),
		vector<string>{config.get<string>("source")},
		config.get<string>("output"),
		cmakeResult));

	result.message = createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	if(cmakeResult.exitCode == 0)
	{
		TextProcessResult makeResult = executeTextProcess(
			config.get<string>("make:binary"),
			vector<string>{ },
			config.get<string>("output"));

		result.output.emplace_back("make", createTaskOutput(
			config.get<string>("make:binary"),
			vector<string>{ },
			config.get<string>("output"),
			makeResult));

		result.message = createTaskMessage(makeResult);

		result.warnings = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const pair<TextProcessResult::LineType, string>& el) -> unsigned int {
				return count + (el.second.find(": warning:") != string::npos ? 1 : 0);
			});

		result.errors = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const pair<TextProcessResult::LineType, string>& el) -> unsigned int {
				return count + (el.second.find(": error:") != string::npos ? 1 : 0);
			});

		result.status =
			((result.errors > 0 || makeResult.exitCode != 0)
				? TaskResult::STATUS_ERROR
				: (result.warnings > 0
					? TaskResult::STATUS_WARNING
					: TaskResult::STATUS_OK));
	}

	return result;
}


TaskResult task_test_googletest( const ptree& config )
{
	TaskResult result;

	// prepare parameter and dirs
	boost::filesystem::path xmlFilePath = config.get<string>("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	vector<string> arguments = {string("--gtest_output=xml:")+xmlFilePath.string()};

	// run test
	TextProcessResult testResult = executeTextProcess(config.get<string>("binary"), arguments, parentPath.string());

	// read XML result file
	ptree xmlTestResult;

	ifstream xmlTestResultStream;
	xmlTestResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	xmlTestResultStream.open( xmlFilePath.string() );

	read_xml( xmlTestResultStream, xmlTestResult, xml_parser::trim_whitespace );

	// generate outline table
	js::Object table_outline;

	{
		js::Object row;
		row.emplace_back("name", xmlTestResult.get<string>("testsuites.<xmlattr>.name"));
		row.emplace_back("tests", xmlTestResult.get<string>("testsuites.<xmlattr>.tests"));
		row.emplace_back("failures", xmlTestResult.get<string>("testsuites.<xmlattr>.failures"));
		row.emplace_back("errors", xmlTestResult.get<string>("testsuites.<xmlattr>.errors"));
		row.emplace_back("disabled", xmlTestResult.get<string>("testsuites.<xmlattr>.disabled"));
		row.emplace_back("time", xmlTestResult.get<string>("testsuites.<xmlattr>.time"));
		row.emplace_back("status", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.emplace_back("all", row);
	}

	js::Array testsuites;
	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == string("<xmlattr>"))
			continue;

		js::Object row;
		row.emplace_back("name", testsuite.second.get<string>("<xmlattr>.name"));
		row.emplace_back("tests", testsuite.second.get<string>("<xmlattr>.tests"));
		row.emplace_back("failures", testsuite.second.get<string>("<xmlattr>.failures"));
		row.emplace_back("errors", testsuite.second.get<string>("<xmlattr>.errors"));
		row.emplace_back("disabled", testsuite.second.get<string>("<xmlattr>.disabled"));
		row.emplace_back("time", testsuite.second.get<string>("<xmlattr>.time"));
		row.emplace_back("status", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		testsuites.push_back(row);
	}
	table_outline.emplace_back("details", testsuites);
	result.output.emplace_back("testsuites", table_outline);

	// generate details table
	js::Array table_details;
	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == string("<xmlattr>"))
			continue;

		for ( auto& testcase : testsuite.second )
		{
			if(testcase.first == string("<xmlattr>"))
				continue;

			js::Object row;
			row.emplace_back("name", testcase.second.get<string>("<xmlattr>.classname")+string(".")+testcase.second.get<string>("<xmlattr>.name"));
			row.emplace_back("status", testcase.second.get<string>("<xmlattr>.status"));
			row.emplace_back("failure", (testcase.second.count("failure") > 0 ? testcase.second.get<string>("failure.<xmlattr>.message") : string("")));
			row.emplace_back("time", testcase.second.get<string>("<xmlattr>.time"));
			row.emplace_back("count", (testcase.second.count("failure") > 0 ? "Error" : "Ok"));

			table_details.push_back(row);
		}
	}

	result.output.emplace_back("tests", table_details);

	// generate console output
	for(auto& line : testResult.output)
	{
		if(line.second.find("[  FAILED  ]") != string::npos)
		{
			line.first = TextProcessResult::ERROR_LINE;
		}
		else
		if(line.second.find("[       OK ]") != string::npos || line.second.find("[  PASSED  ]") != string::npos)
		{
			line.first = TextProcessResult::OK_LINE;
		}
	}

	result.output.emplace_back("googletest", createTaskOutput(config.get<string>("binary"), arguments, parentPath.string(), testResult));

	// generate meta data
	result.message = createTaskMessage(testResult);
	result.warnings = 0;
	result.errors = xmlTestResult.get<int>("testsuites.<xmlattr>.failures") + xmlTestResult.get<int>("testsuites.<xmlattr>.errors");
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}


TaskResult task_test_cppcheck( const ptree& config )
{
	TaskResult result;

	result.output.emplace_back("cppcheck", "under construction...");
	result.message = "under construction...";
	result.warnings = 0;
	result.errors = 1;
	result.status = TaskResult::STATUS_ERROR;

	return result;
}


TaskResult task_doc_doxygen( const ptree& config )
{
	TaskResult result;

	// prepare arguments and dirs

	string sourcePath = config.get<string>("source");
	string outputPath = config.get<string>("output");
	string doxyfilePath = (boost::filesystem::path(outputPath) / boost::filesystem::path("doxyfile")).string();

	boost::filesystem::remove_all(outputPath);
	boost::filesystem::create_directories(outputPath);

	// generate doxyfile

	ofstream doxyfileStream;
	doxyfileStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	doxyfileStream.open( doxyfilePath );

	for( auto& data : config.get_child("doxyfile") )
	{
		doxyfileStream << data.first << " = " << data.second.data() << endl;
	}

	doxyfileStream << "INPUT" << " = " << sourcePath << endl;
	doxyfileStream << "OUTPUT_DIRECTORY" << " = " << outputPath << endl;

	doxyfileStream.close();

	// run doxygen
	TextProcessResult doxygenResult = executeTextProcess(config.get<string>("binary"), vector<string>{doxyfilePath}, outputPath);

	// generate task output
	result.output.emplace_back("doxygen", createTaskOutput(config.get<string>("binary"), vector<string>{doxyfilePath}, outputPath, doxygenResult));

	// generate meta data
	result.message = createTaskMessage(doxygenResult);
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
