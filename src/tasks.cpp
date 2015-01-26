
#include <numeric>
#include <fstream>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <boost/algorithm/string/replace.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "tasks.hpp"
#include "process.hpp"
#include "task_utils.hpp"

namespace tasks {

namespace pt = boost::property_tree;
namespace js = json_spirit;

TaskResult task_build_cmake    ( config::ConfigNode config );
TaskResult task_test_googletest( config::ConfigNode config );
TaskResult task_analysis_cppcheck  ( config::ConfigNode config );
TaskResult task_doc_doxygen    ( config::ConfigNode config );
TaskResult task_publish_rsync  ( config::ConfigNode config );

std::map<std::string, std::function<TaskResult(config::ConfigNode)>> taskTypes =
{
	{ "build:cmake",       task_build_cmake },
	{ "test:googletest",   task_test_googletest },
	{ "analysis:cppcheck", task_analysis_cppcheck },
	{ "doc:doxygen",       task_doc_doxygen },
	{ "publish:rsync",     task_publish_rsync }
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

TaskResult task_build_cmake( config::ConfigNode config )
{
	// create directories
	boost::filesystem::create_directories(config.value("build.output"));
	boost::filesystem::create_directories(config.value("install.output"));

	// run cmake
	TaskResult result;

	std::vector<std::string> cmakeParams {

		std::string("-DCMAKE_C_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("target.c.binary"), "\\", "/"),
		std::string("-DCMAKE_CXX_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("target.c++.binary"), "\\", "/"),
		std::string("-DTARGET_ARCHITECTURE:STRING=") + config.value("target.architecture"),
		std::string("-DTARGET_BITNESS:STRING=") + config.value("target.bitness"),
		std::string("-DTARGET_OS:STRING=") + config.value("target.os"),

		std::string("-DLOCAL_C_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("local.c.binary"), "\\", "/"),
		std::string("-DLOCAL_CXX_COMPILER:STRING=") + boost::algorithm::replace_all_copy(config.value("local.c++.binary"), "\\", "/"),
		std::string("-DLOCAL_ARCHITECTURE:STRING=") + config.value("local.architecture"),
		std::string("-DLOCAL_BITNESS:STRING=") + config.value("local.bitness"),
		std::string("-DLOCAL_OS:STRING=") + config.value("local.os"),

		std::string("-DCMAKE_INSTALL_PREFIX:STRING=") + boost::algorithm::replace_all_copy(config.value("install.output"), "\\", "/")
	};

	if(config.value("verbose") == "yes")
		{ cmakeParams.push_back(std::string("-DCMAKE_VERBOSE_MAKEFILE:BOOLEAN=ON")); }

	for(auto variable : config.node("cmake.variables").children())
	{
		cmakeParams.push_back( std::string("-D") + variable.first + std::string(":")
			+ variable.second.value("type") + std::string("=")
			+ variable.second.value("value") );
	}

	cmakeParams.push_back(boost::algorithm::replace_all_copy(config.value("source"), "\\", "/"));

#ifdef _WIN32
	cmakeParams.push_back("-G");
	cmakeParams.push_back(config.value("cmake.generator"));
#endif

	process::TextProcessResult cmakeResult = process::executeTextProcess(
		config.value("cmake.binary"),
		cmakeParams,
		config.value("build.output"));

	result.output.emplace_back("cmake", task_utils::createTaskOutput(
		config.value("cmake.binary"),
		cmakeParams,
		config.value("build.output"),
		cmakeResult));

	result.message = task_utils::createTaskMessage(cmakeResult);
	result.warnings = 0;
	result.errors = (cmakeResult.exitCode != 0 ? 1 : 0);
	result.status = (cmakeResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	// run make
	if(cmakeResult.exitCode == 0)
	{
		std::vector<std::string> makeParams;

		for(auto variable : config.node("make.variables").children())
		{
			makeParams.push_back( variable.first + std::string("=") + variable.second.value() );
		}

		process::TextProcessResult makeResult = process::executeTextProcess(
			config.value("make.binary"),
			makeParams,
			config.value("build.output"));

		result.output.emplace_back("make", task_utils::createTaskOutput(
			config.value("make.binary"),
			makeParams,
			config.value("build.output"),
			makeResult));

		result.message = task_utils::createTaskMessage(makeResult);

		result.warnings = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<process::TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": warning:") != std::string::npos ? 1 : 0);
			});

		result.errors = accumulate(
			makeResult.output.begin(), makeResult.output.end(), 0,
			[] (unsigned int count, const std::pair<process::TextProcessResult::LineType, std::string>& el) -> unsigned int {
				return count + (el.second.find(": error:") != std::string::npos ? 1 : 0);
			});

		result.status =
			((result.errors > 0 || makeResult.exitCode != 0)
				? TaskResult::STATUS_ERROR
				: (result.warnings > 0
					? TaskResult::STATUS_WARNING
					: TaskResult::STATUS_OK));

		// run install
		if(makeResult.exitCode == 0 && config.value("install.enabled") == "yes")
		{
			std::vector<std::string> installParams{ "install" };

			for(auto variable : config.node("make.variables").children())
			{
				installParams.push_back( variable.first + std::string("=") + variable.second.value() );
			}

			process::TextProcessResult installResult = process::executeTextProcess(
				config.value("make.binary"),
				installParams,
				config.value("install.base"));

			result.output.emplace_back("install", task_utils::createTaskOutput(
				config.value("make.binary"),
				installParams,
				config.value("install.base"),
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


TaskResult task_test_googletest( config::ConfigNode config )
{
	TaskResult result;

	// prepare parameter and dirs
	boost::filesystem::path xmlFilePath = config.value("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	std::vector<std::string> arguments = {
		"--gtest_output=xml:" + xmlFilePath.string(),
		"--gtest_filter=" + config.value("filter")
	};

	// run test
	process::TextProcessResult testResult = process::executeTextProcess(config.value("binary"), arguments, parentPath.string());

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
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
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
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		for ( auto testcase : testsuite.second )
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

	result.output.emplace_back("googletest", task_utils::createTaskOutput(config.value("binary"), arguments, parentPath.string(), testResult));

	// generate meta data
	result.message = task_utils::createTaskMessage(testResult);
	result.warnings = 0;
	result.errors = xmlTestResult.get<int>("testsuites.<xmlattr>.failures") + xmlTestResult.get<int>("testsuites.<xmlattr>.errors");
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	return result;
}


TaskResult task_analysis_cppcheck( config::ConfigNode config )
{
	boost::filesystem::path xmlFilePath = config.value("output");
	boost::filesystem::path parentPath = xmlFilePath.branch_path();

	boost::filesystem::create_directories(parentPath);

	TaskResult result;

	std::vector<std::string> arguments { "--xml-version=2", "--enable=all", "--suppress=missingIncludeSystem", "." };

	process::TextProcessResult checkResult = process::executeTextProcess(
		config.value("binary"),
		arguments,
		config.value("source"));

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
		xmlCheckPersistStream.open( config.value("output") );

		xmlCheckPersistStream << xmlCheckData;
		xmlCheckPersistStream.close();

		// read XML result data
		pt::ptree xmlCheckResult;

		std::istringstream xmlCheckResultStream(xmlCheckData);
		xmlCheckResultStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

		pt::read_xml( xmlCheckResultStream, xmlCheckResult, pt::xml_parser::trim_whitespace );

		// interpret xml data
		js::Array errors;
		for ( auto error : xmlCheckResult.get_child("results.errors") )
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

	result.output.emplace_back("cppcheck", task_utils::createTaskOutput(
		config.value("binary"),
		arguments,
		config.value("source"),
		checkResult));

	result.message = task_utils::createTaskMessage(checkResult);
	result.status = (result.errors > 0 ? TaskResult::STATUS_ERROR : (result.warnings > 0 ?  TaskResult::STATUS_WARNING : TaskResult::STATUS_OK));

	return result;
}


TaskResult task_doc_doxygen( config::ConfigNode config )
{
	TaskResult result;

	// prepare arguments and dirs

	const std::string sourcePath = config.value("source");
	const std::string outputPath = config.value("output");
	const std::string doxyfilePath = (boost::filesystem::path(outputPath) / boost::filesystem::path("doxyfile")).string();

	boost::filesystem::create_directories(outputPath);

	// generate doxyfile

	std::ofstream doxyfileStream;
	doxyfileStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	doxyfileStream.open( doxyfilePath );

	for( auto data : config.node("doxyfile").children() )
	{
		doxyfileStream << data.first << " = " << data.second.value() << '\n';
	}

	doxyfileStream << "INPUT" << " = " << sourcePath << '\n';
	doxyfileStream << "OUTPUT_DIRECTORY" << " = " << outputPath << '\n';

	doxyfileStream.close();

	// run doxygen
	process::TextProcessResult doxygenResult = process::executeTextProcess(config.value("binary"), std::vector<std::string>{doxyfilePath}, outputPath);

	result.output.emplace_back("doxygen", task_utils::createTaskOutput(config.value("binary"), std::vector<std::string>{doxyfilePath}, outputPath, doxygenResult));

	result.message = task_utils::createTaskMessage(doxygenResult);
	result.warnings = 0;
	result.errors = (doxygenResult.exitCode != 0 ? 1 : 0);
	result.status = (doxygenResult.exitCode != 0 ? TaskResult::STATUS_ERROR : TaskResult::STATUS_OK);

	// install docs
	if(doxygenResult.exitCode == 0)
	{
		auto format = config.value("install.format");

		std::cout << "Installing " <<format << " docs..." << std::endl;
		boost::filesystem::create_directories(config.value("install.directory"));

		if(!copyDir(config.value("output") + "/" +format, config.value("install.directory") + "/" +format))
		{
			result.errors += 1;
			result.status = TaskResult::STATUS_ERROR;
			result.message = std::string("Could not copy ") +format + std::string(" format output.");
		}
	}

	return result;
}

TaskResult task_publish_rsync( config::ConfigNode config )
{
	TaskResult result;

	// run ssh:mkdir

	std::vector<std::string> sshArgs {
		"-o", "BatchMode=yes", "-p", config.value("destination.port"),
		config.value("destination.user") + std::string("@") + config.value("destination.host"),
		"mkdir", "-p",
		config.value("destination.base") + std::string("/") + config.value("destination.directory")
	};

	process::TextProcessResult sshResult = process::executeTextProcess(config.value("ssh.binary"), sshArgs, config.value("source"));

	result.output.emplace_back("ssh:mkdir", task_utils::createTaskOutput(config.value("ssh.binary"), sshArgs, config.value("source"), sshResult));

	if(sshResult.exitCode != 0)
	{
		result.message = task_utils::createTaskMessage(sshResult);
		result.warnings = 0;
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;

		return result;
	}

	// run rsync
	std::vector<std::string> rsyncArgs {
		std::string("--rsh=") + config.value("ssh.binary") + (" -o BatchMode=yes -p ") + config.value("destination.port"),
		"--archive", "--delete", "--verbose", std::string("./"),
		config.value("destination.user") + std::string("@") + config.value("destination.host") + std::string(":")
		+ config.value("destination.base") + std::string("/") + config.value("destination.directory") + std::string("/")
	};

	process::TextProcessResult rsyncResult = process::executeTextProcess(config.value("rsync.binary"), rsyncArgs, config.value("source"));

	result.output.emplace_back("rsync", task_utils::createTaskOutput(config.value("rsync.binary"), rsyncArgs, config.value("source"), rsyncResult));

	// generate meta data
	result.message = task_utils::createTaskMessage(rsyncResult);
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

} // namespace: tasks
