#include <numeric>
#include <fstream>
#include <set>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "tasks.hpp"
#include "process.hpp"
#include "task_utils.hpp"
#include "formatter.hpp"

namespace tasks {

TaskResult task_build_cmake             ( uon::Value config );
TaskResult task_test_googletest         ( uon::Value config );
TaskResult task_analysis_cppcheck       ( uon::Value config );
TaskResult task_doc_doxygen             ( uon::Value config );
TaskResult task_publish_rsync_ssh       ( uon::Value config );
TaskResult task_publish_mongo_rsync_ssh ( uon::Value config );
TaskResult task_publish_email           ( uon::Value config );

std::map<std::string, std::function<TaskResult(uon::Value)>> taskTypes =
{
	{ "build:cmake",             task_build_cmake },
	{ "test:googletest",         task_test_googletest },
	{ "analysis:cppcheck",       task_analysis_cppcheck },
	{ "doc:doxygen",             task_doc_doxygen },
	{ "publish:rsync+ssh",       task_publish_rsync_ssh },
	{ "publish:mongo+rsync+ssh", task_publish_mongo_rsync_ssh },
	{ "publish:email",           task_publish_email }
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
			makeParams.push_back( variable.first + std::string("=") + variable.second.to_string() );
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
				details_row.set("row", row);
				details_row.set("column", column);

				details.push_back(details_row);
			}
		}

		result.output.set("results", details);

		result.output.set("make", task_utils::createTaskOutput(
			config.get("make.binary").to_string(),
			makeParams,
			config.get("build.output").to_string(),
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
		row.set("tests", xmlTestResult.get<std::string>("testsuites.<xmlattr>.tests"));
		row.set("failures", xmlTestResult.get<std::string>("testsuites.<xmlattr>.failures"));
		row.set("errors", xmlTestResult.get<std::string>("testsuites.<xmlattr>.errors"));
		row.set("disabled", xmlTestResult.get<std::string>("testsuites.<xmlattr>.disabled"));
		row.set("time", xmlTestResult.get<std::string>("testsuites.<xmlattr>.time"));
		row.set("result", ((xmlTestResult.get<int>("testsuites.<xmlattr>.failures") > 0 || xmlTestResult.get<int>("testsuites.<xmlattr>.errors") > 0) ? "Error" : "Ok"));

		table_outline.set("all", row);
	}

	uon::Value testsuites;
	for ( auto testsuite : xmlTestResult.get_child("testsuites") )
	{
		if(testsuite.first == "<xmlattr>")
			continue;

		uon::Value row;
		row.set("name", testsuite.second.get<std::string>("<xmlattr>.name"));
		row.set("tests", testsuite.second.get<std::string>("<xmlattr>.tests"));
		row.set("failures", testsuite.second.get<std::string>("<xmlattr>.failures"));
		row.set("errors", testsuite.second.get<std::string>("<xmlattr>.errors"));
		row.set("disabled", testsuite.second.get<std::string>("<xmlattr>.disabled"));
		row.set("time", testsuite.second.get<std::string>("<xmlattr>.time"));
		row.set("result", ((testsuite.second.get<int>("<xmlattr>.failures") > 0 || testsuite.second.get<int>("<xmlattr>.errors") > 0) ? "Error" : "Ok"));

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
			row.set("message", (testcase.second.count("failure") > 0 ? testcase.second.get<std::string>("failure.<xmlattr>.message") : ""));
			row.set("time", testcase.second.get<std::string>("<xmlattr>.time"));
			row.set("result", (testcase.second.count("failure") > 0 ? "Error" : "Ok"));

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

	std::vector<std::string> arguments { "--xml-version=2", "--enable=all", "--suppress=missingIncludeSystem", "." };

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
		uon::Array errors;
		for ( auto error : xmlCheckResult.get_child("results.errors") )
		{
			if(error.first == "<xmlattr>")
				continue;

			auto attr_file = error.second.get_optional<std::string>("location.<xmlattr>.file");
			auto attr_line = error.second.get_optional<std::string>("location.<xmlattr>.line");

			uon::Value row;
			row.set("type", error.second.get<std::string>("<xmlattr>.id"));
			row.set("severity", error.second.get<std::string>("<xmlattr>.severity"));
			row.set("message", error.second.get<std::string>("<xmlattr>.msg"));
			row.set("file", attr_file ? *attr_file : std::string(""));
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

TaskResult task_publish_rsync_ssh( uon::Value config )
{
	TaskResult result;

	for( auto source : config.get("sources").as_object() )
	{
		std::string srcdir = source.second.to_string();
		std::string remote = config.get("destination.user").to_string() + std::string("@") + config.get("destination.host").to_string();
		std::string destdir = config.get("destination.base").to_string() + std::string("/") + config.get("destination.directory").to_string() + std::string("/") + source.first;

		// run ssh:mkdir
		std::vector<std::string> sshArgs { "-o", "BatchMode=yes", "-p", config.get("destination.port").to_string(), remote, config.get("destination.mkdir.binary").to_string(), "-p", destdir };

		process::TextProcessResult sshResult = process::executeTextProcess(config.get("ssh.binary").to_string(), sshArgs, srcdir);

		result.output.set("ssh:mkdir", task_utils::createTaskOutput(config.get("ssh.binary").to_string(), sshArgs, srcdir, sshResult));
		result.message = task_utils::createTaskMessage(sshResult);

		if(sshResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}

		// run rsync
		std::vector<std::string> rsyncArgs {
			std::string("--rsh=") + config.get("ssh.binary").to_string() + (" -o BatchMode=yes -p ") + config.get("destination.port").to_string(), "--archive", "--delete", "--verbose", std::string("./"),
			remote + std::string(":") + destdir + std::string("/")
		};

		process::TextProcessResult rsyncResult = process::executeTextProcess(config.get("rsync.binary").to_string(), rsyncArgs, srcdir);

		result.output.set("rsync", task_utils::createTaskOutput(config.get("rsync.binary").to_string(), rsyncArgs, srcdir, rsyncResult));
		result.message = task_utils::createTaskMessage(rsyncResult);

		if(rsyncResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}
	}

	result.warnings = 0;
	result.errors = 0;
	result.status = TaskResult::STATUS_OK;

	return result;
}

TaskResult task_publish_mongo_rsync_ssh( uon::Value config )
{
	// rsync stuff
	TaskResult result = task_publish_rsync_ssh(config);

	if(result.status == TaskResult::STATUS_ERROR)
	{
		return result;
	}

	// import to mongo database
	for( auto source : config.get("sources").as_object() )
	{
		std::string srcdir = source.second.to_string();
		std::string remote = config.get("destination.user").to_string() + std::string("@") + config.get("destination.host").to_string();
		std::string destfile = config.get("destination.base").to_string() + std::string("/") + config.get("destination.directory").to_string() + std::string("/") + source.first;

		// run ssh:mongoimport
		std::vector<std::string> mongoArgs { "-o", "BatchMode=yes", "-p", config.get("destination.port").to_string(), remote,
			config.get("destination.mongoimport.binary").to_string(),
			"--db", config.get("destination.mongoimport.database").to_string(),
			"--collection", config.get("destination.mongoimport.collection").to_string(),
			destfile };

		process::TextProcessResult mongoResult = process::executeTextProcess(config.get("ssh.binary").to_string(), mongoArgs, srcdir);

		result.output.set("ssh:mongoimport", task_utils::createTaskOutput(config.get("ssh.binary").to_string(), mongoArgs, srcdir, mongoResult));
		result.message = task_utils::createTaskMessage(mongoResult);

		if(mongoResult.exitCode != 0)
		{
			result.errors = 1;
			result.status = TaskResult::STATUS_ERROR;
			return result;
		}
	}

	return result;
}

TaskResult task_publish_email( uon::Value config )
{
	TaskResult result;
	result.warnings = 0;
	result.errors = 0;

	// detect authors
	process::TextProcessResult gitAuthors = process::executeTextProcess(
		config.get("git.binary").to_string(), {"log", "--pretty=%ae", std::string("--since=")+config.get("receivers.authors").to_string()},
		config.get("receivers.repository").to_string());

	result.message = task_utils::createTaskMessage(gitAuthors);

	if(gitAuthors.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	// detect committers
	process::TextProcessResult gitCommitters = process::executeTextProcess(
		config.get("git.binary").to_string(), {"log", "--pretty=%ae", std::string("--since=")+config.get("receivers.committers").to_string()},
		config.get("receivers.repository").to_string());

	result.message = task_utils::createTaskMessage(gitCommitters);

	if(gitCommitters.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	// generate set of receivers
	std::set<std::string> receivers;

	for(auto receiver : gitAuthors.output)
	{
		if(    receiver.second.length() > 0
			&& receiver.first == process::TextProcessResult::LineType::INFO_LINE
			&& receivers.find(receiver.second) == receivers.end())
		{
			receivers.insert(receiver.second);
		}
	}

	for(auto receiver : gitCommitters.output)
	{
		if(    receiver.second.length() > 0
			&& receiver.first == process::TextProcessResult::LineType::INFO_LINE
			&& receivers.find(receiver.second) == receivers.end())
		{
			receivers.insert(receiver.second);
		}
	}

	if(receivers.size() == 0)
	{
		result.status = TaskResult::STATUS_OK;
		return result;
	}

	// generate message
	std::stringstream message;

	for(auto report : config.get("reports").as_object())
	{
		std::cout << "reading " << report.second.to_string() << "..." << std::endl;

		// load report
		std::ifstream reportstream;
		reportstream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		reportstream.open( report.second.to_string() );

		uon::Value reporttree = uon::read_json( reportstream );

		// format report
		std::cout << "formatting markdown..." << std::endl;
		formatter::markdown(reporttree, message);
	}

	// send mail
	std::vector<std::string> mailArgs{
		"-a", "Content-Type: text/plain; charset=UTF-8",
		"-s", config.get("subject").to_string()
	};
	mailArgs.insert(mailArgs.end(), receivers.begin(), receivers.end());

	process::TextProcessResult mailResult = process::executeTextProcess(
		config.get("mail.binary").to_string(), mailArgs,
		config.get("receivers.repository").to_string(),
		message.str());

	result.message = task_utils::createTaskMessage(mailResult);

	if(mailResult.exitCode != 0)
	{
		result.errors = 1;
		result.status = TaskResult::STATUS_ERROR;
		return result;
	}

	result.status = TaskResult::STATUS_OK;
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
