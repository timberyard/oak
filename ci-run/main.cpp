
#include <fstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/system/system_error.hpp>

#include "ptree.utils.hpp"
#include "tasks.hpp"

namespace pt = boost::property_tree;

namespace Json {
	void test();
}

int main( int argc, const char* const* argv )
{
	//Json::test();
	//return 0;

	// read configuration
	pt::ptree config;

	try
	{
		for ( int i = 1; i < argc; ++i )
		{
			if ( strlen(argv[i]) == 0 || argv[i][0] == '-' )
				continue;

			std::ifstream configStream;

			configStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
			configStream.open( argv[i] );

			pt::ptree configTree;
			read_json( configStream, configTree );

			ptree_merge( config, configTree );
		}
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << exception.what() << '\n';
		return -1;
	}
	catch ( const std::ifstream::failure& exception )
	{
		std::cerr << exception.what() << '\n';
		return -1;
	}

	// read template

	pt::ptree output;

	try
	{
		// read template
		std::string outputTemplatePath = config.get<std::string>("output.template.file");

		std::ifstream outputTemplateStream;

		outputTemplateStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		outputTemplateStream.open( outputTemplatePath );

		pt::read_xml( outputTemplateStream, output, pt::xml_parser::trim_whitespace );
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << exception.what() << '\n';
		return -1;
	}
	catch ( const std::ifstream::failure& exception )
	{
		std::cerr << exception.what() << '\n';
		return -1;
	}


	// run configurations

	pt::ptree outputTasks;

	try
	{
		outputTasks.put("div.<xmlattr>.class", "table-responsive");
		outputTasks.put("div.table.<xmlattr>.class", "tasks table table-bordered");

		outputTasks.add_child("div.table.thead.tr", pt::ptree());
		outputTasks.add_child("div.table.tbody", pt::ptree());

		outputTasks.get_child("div.table.thead.tr").add("th", "Type");
		outputTasks.get_child("div.table.thead.tr").add("th", "Name");
		outputTasks.get_child("div.table.thead.tr").add("th", "Message");
		outputTasks.get_child("div.table.thead.tr").add("th", "Warnings");
		outputTasks.get_child("div.table.thead.tr").add("th", "Errors");
		outputTasks.get_child("div.table.thead.tr").add("th", "Status");

		for ( auto& taskConfig : config.get_child("tasks") )
		{
			// get task settings
			std::string taskType = taskConfig.second.get<std::string>( "type" );

			boost::optional<std::string> taskVariant = taskConfig.second.get_optional<std::string>( "variant" );

			if(!taskVariant)
				taskVariant = std::string("defaults");

			boost::optional<pt::ptree&> settings_specific = taskConfig.second.get_child_optional( "settings" );
			boost::optional<pt::ptree&> settings_variant = config.get_child_optional( std::string("defaults.") + taskType + std::string(".") + *taskVariant );

			pt::ptree settings;

			if(settings_variant)
				ptree_merge(settings, *settings_variant);

			if(settings_specific)
				ptree_merge(settings, *settings_specific);

			// expand variables
			ptree_traverse
			(
				settings,
				[] (pt::ptree &parent, const pt::ptree::path_type &childPath, pt::ptree &child)
				{
					boost::replace_all(child.data(), "${source.repository}", "git@jakarta:ci-test");
					boost::replace_all(child.data(), "${source.branch}", "master");
					boost::replace_all(child.data(), "${source.commit.id}", "9a034128a329f4fb7a53043dd1d1e8f74bfc91fc");
					boost::replace_all(child.data(), "${source.commit.timestamp}", "20140520-203412");
					boost::replace_all(child.data(), "${source.path}", "/home/niklas/dev/oak/dummy/source");
					boost::replace_all(child.data(), "${output.path}", "/home/niklas/dev/oak/dummy/output");
				}
			);

			// run task
			auto task = taskTypes.find(taskType);

			if(task != taskTypes.end())
			{
				TaskResult result;

				try
				{
					result = task->second(settings);
				}
				catch(const std::exception& e)
				{
					result.status = TaskResult::STATUS_ERROR;
					result.warnings = 0;
					result.errors = 1;
					result.message = "exception occured";
					result.output.put("pre", e.what());
				}

				std::ostringstream warningsStr; warningsStr << result.warnings;
				std::ostringstream errorsStr; errorsStr << result.errors;

				pt::ptree outputTask;

				outputTask.put("<xmlattr>.class", std::string("task-meta ") + (result.status == TaskResult::STATUS_OK ? "status-ok" : (result.status == TaskResult::STATUS_WARNING ? "status-warning" : "status-error")));
				outputTask.add("td", taskType);
				outputTask.add("td", taskConfig.first);
				outputTask.add("td", result.message);
				outputTask.add("td", warningsStr.str());
				outputTask.add("td", errorsStr.str());
				outputTask.add("td", result.status == TaskResult::STATUS_OK ? "Ok" : (result.status == TaskResult::STATUS_WARNING ? "Warning" : "Error"));

				outputTasks.get_child("div.table.tbody").add_child("tr", outputTask);

				result.output.put("<xmlattr>.colspan", "6");

				pt::ptree& outputTaskRow = outputTasks.get_child("div.table.tbody").add_child("tr", pt::ptree());
				outputTaskRow.add_child("td", result.output);
				outputTaskRow.put("<xmlattr>.class", "task-output");
			}
			else
				throw std::runtime_error(std::string("invalid task type: ") + taskType);
		}
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << "PTree Exception: " << exception.what() << '\n';
		return -1;
	}
	catch ( const boost::system::system_error& exception )
	{
		std::cerr << "System Error: " << exception.what() << '\n';
		return -2;
	}
	catch ( const std::runtime_error& exception )
	{
		std::cerr << "Runtime Error: " << exception.what() << '\n';
		return -3;
	}

	// dump output

	try
	{
		output.put(config.get<std::string>("output.template.paths.title"), config.get<std::string>("name"));
		output.put_child(config.get<std::string>("output.template.paths.content"), outputTasks);

		std::ostringstream outputStream;
		write_xml(outputStream, output, pt::xml_writer_settings<char>('\t', 1));

		std::string outputStr = outputStream.str();
		outputStr = outputStr.insert(outputStr.find("?>")+2, "\n<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\t\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">");

		std::cout << outputStr << '\n';
	}
	catch ( const pt::ptree_error& exception )
	{
		std::cerr << exception.what() << '\n';
		return -1;
	}


	return 0;
}
