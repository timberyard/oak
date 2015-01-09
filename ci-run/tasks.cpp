
#include <numeric>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "tasks.hpp"
#include "process.hpp"
#include "task.utils.hpp"

using namespace std;
using namespace boost::property_tree;

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

extern std::string toolchain;

TaskResult task_build_cmake( const ptree& config )
{
	boost::filesystem::remove_all(config.get<string>("output"));
	boost::filesystem::create_directories(config.get<string>("output"));

	TaskResult result;

	vector<string> cmakeParams { config.get<string>("source") };

#ifndef _WIN32
	if(toolchain == "gcc")
	{
		cmakeParams.insert(cmakeParams.begin(), "-DCMAKE_C_COMPILER:STRING=gcc");
		cmakeParams.insert(cmakeParams.begin(), "-DCMAKE_CXX_COMPILER:STRING=g++");
	}
	else
	if(toolchain == "clang")
	{
		cmakeParams.insert(cmakeParams.begin(), "-DCMAKE_C_COMPILER:STRING=clang");
		cmakeParams.insert(cmakeParams.begin(), "-DCMAKE_CXX_COMPILER:STRING=clang++");
	}
#else
	if(toolchain == "gcc")
	{
		cmakeParams.push_back("-G");
		cmakeParams.push_back("MSYS Makefiles");
	}
	else
	if(toolchain == "msvc")
	{
		// do nothing (since msvc is default on windows)
	}
#endif
	else
	{
		throw std::runtime_error(std::string("invalid toolchain: ") + toolchain);
	}

	TextProcessResult cmakeResult = executeTextProcess(
		config.get<string>("binary:cmake"),
		cmakeParams,
		config.get<string>("output"));

	result.output.add_child("div", createTaskOutput(
		config.get<string>("binary:cmake"),
		vector<string>{config.get<string>("source")},
		config.get<string>("output"),
		cmakeResult))
		.put("<xmlattr>.class", "task-output-block");

	result.message = createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	if(cmakeResult.exitCode == 0)
	{
		TextProcessResult makeResult = executeTextProcess(
			config.get<string>("binary:make"),
			vector<string>{ },
			config.get<string>("output"));

		result.output.add_child("div", createTaskOutput(
			config.get<string>("binary:make"),
			vector<string>{ },
			config.get<string>("output"),
			makeResult))
			.put("<xmlattr>.class", "task-output-block");

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
	ptree table_outline;

	table_outline.put("table.<xmlattr>.class", "table table-bordered table-condensed");

	table_outline.add_child("table.thead.tr", ptree());
	table_outline.add_child("table.tbody", ptree());

	table_outline.get_child("table.thead.tr").add("th", "Name");
	table_outline.get_child("table.thead.tr").add("th", "Tests");
	table_outline.get_child("table.thead.tr").add("th", "Failures");
	table_outline.get_child("table.thead.tr").add("th", "Errors");
	table_outline.get_child("table.thead.tr").add("th", "Disabled");
	table_outline.get_child("table.thead.tr").add("th", "Time");
	table_outline.get_child("table.thead.tr").add("th", "Status");

	{	
		ptree row;
		row.put("<xmlattr>.class", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "status-error" : "status-ok"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.name"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.tests"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.failures"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.errors"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.disabled"));
		row.add("td", xmlTestResult.get<string>("testsuites.<xmlattr>.time"));
		row.add("td", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.get_child("table.tbody").add_child("tr", row);
	}

	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == string("<xmlattr>"))
			continue;

		ptree row;
		row.put("<xmlattr>.class", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "status-error" : "status-ok"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.name"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.tests"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.failures"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.errors"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.disabled"));
		row.add("td", testsuite.second.get<string>("<xmlattr>.time"));
		row.add("td", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.get_child("table.tbody").add_child("tr", row);
	}

	result.output.add_child("div", table_outline);

	// generate details table
	ptree table_details;

	table_details.put("table.<xmlattr>.class", "table table-bordered table-condensed");

	table_details.add_child("table.thead.tr", ptree());
	table_details.add_child("table.tbody", ptree());

	table_details.get_child("table.thead.tr").add("th", "Name");
	table_details.get_child("table.thead.tr").add("th", "Status");
	table_details.get_child("table.thead.tr").add("th", "Message");
	table_details.get_child("table.thead.tr").add("th", "Time");
	table_details.get_child("table.thead.tr").add("th", "Status");

	for ( auto& testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == string("<xmlattr>"))
			continue;

		for ( auto& testcase : testsuite.second )
		{
			if(testcase.first == string("<xmlattr>"))
				continue;

			ptree row;
			row.put("<xmlattr>.class", (testcase.second.count("failure") > 0 ? "status-error" : "status-ok"));
			row.add("td", testcase.second.get<string>("<xmlattr>.classname")+string(".")+testcase.second.get<string>("<xmlattr>.name"));
			row.add("td", testcase.second.get<string>("<xmlattr>.status"));
			row.add("td", (testcase.second.count("failure") > 0 ? testcase.second.get<string>("failure.<xmlattr>.message") : string("")));
			row.add("td", testcase.second.get<string>("<xmlattr>.time"));
			row.add("td", (testcase.second.count("failure") > 0 ? "Error" : "Ok"));

			table_details.get_child("table.tbody").add_child("tr", row);
		}
	}

	result.output.add_child("div", table_details);

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

	result.output.add_child("div", createTaskOutput(config.get<string>("binary"), arguments, parentPath.string(), testResult))
		.put("<xmlattr>.class", "task-output-block");

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

	result.output.put("pre", "under construction...");
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

	// generate console output
	for(auto& line : doxygenResult.output)
	{
		/*
		if(line.second.find("[  FAILED  ]") != string::npos)
		{
			line.first = TextProcessResult::ERROR_LINE;
		}
		else
		if(line.second.find("[       OK ]") != string::npos || line.second.find("[  PASSED  ]") != string::npos)
		{
			line.first = TextProcessResult::OK_LINE;
		}
		*/
	}

	result.output.add_child("div", createTaskOutput(config.get<string>("binary"), vector<string>{doxyfilePath}, outputPath, doxygenResult))
		.put("<xmlattr>.class", "task-output-block");

	// generate meta data
	result.message = createTaskMessage(doxygenResult);
	result.warnings = 0;
	result.errors = (doxygenResult.exitCode != 0 ? 1 : 0);
	result.status = (doxygenResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}
