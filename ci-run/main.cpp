
#include <fstream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/system/system_error.hpp>

#include "ptree.utils.hpp"
#include "tasks.hpp"

using namespace std;
using namespace boost::property_tree;

namespace Json {
	void test();
}

int main( int argc, const char* const* argv )
{
	//Json::test();
	//return 0;

	// read configuration
	ptree config;

	try
	{
		for ( int i = 1; i < argc; ++i )
		{
			if ( strlen(argv[i]) == 0 || argv[i][0] == '-' )
				continue;

			ifstream configStream;

			configStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
			configStream.open( argv[i] );

			ptree configTree;
			read_json( configStream, configTree );

			ptree_merge( config, configTree );
		}
	}
	catch ( const ptree_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}
	catch ( const ifstream::failure& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}

	// read template

	ptree output;

	try
	{
		// read template
		string outputTemplatePath = config.get<string>("output.template.file");

		ifstream outputTemplateStream;

		outputTemplateStream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
		outputTemplateStream.open( outputTemplatePath );

		read_xml( outputTemplateStream, output, xml_parser::trim_whitespace );
	}
	catch ( const ptree_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}
	catch ( const ifstream::failure& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}


	// run configurations

	ptree outputTasks;

	try
	{
		outputTasks.put("div.<xmlattr>.class", "table-responsive");
		outputTasks.put("div.table.<xmlattr>.class", "tasks table table-bordered");

		outputTasks.add_child("div.table.thead.tr", ptree());
		outputTasks.add_child("div.table.tbody", ptree());

		outputTasks.get_child("div.table.thead.tr").add("th", "Type");
		outputTasks.get_child("div.table.thead.tr").add("th", "Name");
		outputTasks.get_child("div.table.thead.tr").add("th", "Message");
		outputTasks.get_child("div.table.thead.tr").add("th", "Warnings");
		outputTasks.get_child("div.table.thead.tr").add("th", "Errors");
		outputTasks.get_child("div.table.thead.tr").add("th", "Status");

		for ( auto& taskConfig : config.get_child("tasks") )
		{
			// get task settings
			string taskType = taskConfig.second.get<string>( "type" );

			boost::optional<string> taskVariant = taskConfig.second.get_optional<string>( "variant" );

			if(!taskVariant)
				taskVariant = string("defaults");

			boost::optional<ptree&> settings_specific = taskConfig.second.get_child_optional( "settings" );
			boost::optional<ptree&> settings_variant = config.get_child_optional( string("defaults.") + taskType + string(".") + *taskVariant );

			ptree settings;

			if(settings_variant)
				ptree_merge(settings, *settings_variant);

			if(settings_specific)
				ptree_merge(settings, *settings_specific);

			// expand variables
			ptree_traverse
			(
				settings,
				[] (ptree &parent, const ptree::path_type &childPath, ptree &child)
				{
					boost::replace_all(child.data(), "${source.repository}", "git@jakarta:ci-test");
					boost::replace_all(child.data(), "${source.branch}", "master");
					boost::replace_all(child.data(), "${source.commit.id}", "9a034128a329f4fb7a53043dd1d1e8f74bfc91fc");
					boost::replace_all(child.data(), "${source.commit.sid}", "9a03412");
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

				ostringstream warningsStr; warningsStr << result.warnings;
				ostringstream errorsStr; errorsStr << result.errors;

				ptree outputTask;

				outputTask.put("<xmlattr>.class", string("task-meta ") + (result.status == TaskResult::STATUS_OK ? "status-ok" : (result.status == TaskResult::STATUS_WARNING ? "status-warning" : "status-error")));
				outputTask.add("td", taskType);
				outputTask.add("td", taskConfig.first);
				outputTask.add("td", result.message);
				outputTask.add("td", warningsStr.str());
				outputTask.add("td", errorsStr.str());
				outputTask.add("td", result.status == TaskResult::STATUS_OK ? "Ok" : (result.status == TaskResult::STATUS_WARNING ? "Warning" : "Error"));

				outputTasks.get_child("div.table.tbody").add_child("tr", outputTask);

				result.output.put("<xmlattr>.colspan", "6");

				ptree& outputTaskRow = outputTasks.get_child("div.table.tbody").add_child("tr", ptree());
				outputTaskRow.add_child("td", result.output);
				outputTaskRow.put("<xmlattr>.class", "task-output");
			}
			else
				throw runtime_error(string("invalid task type: ") + taskType);
		}
	}
	catch ( const ptree_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}
	catch ( const boost::system::system_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}
	catch ( const runtime_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}

	// dump output

	try
	{
		output.put(config.get<string>("output.template.paths.title"), config.get<string>("name"));
		output.put_child(config.get<string>("output.template.paths.content"), outputTasks);

		ostringstream outputStream;
		write_xml(outputStream, output, xml_writer_settings<char>('\t', 1));

		string outputStr = outputStream.str();
		outputStr = outputStr.insert(outputStr.find("?>")+2, "\n<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\t\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">");

		cout << outputStr << endl;
	}
	catch ( const ptree_error& exception )
	{
		cerr << exception.what() << endl;
		return -1;
	}


	return 0;
}
