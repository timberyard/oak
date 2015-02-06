#include <boost/program_options.hpp>
#include <uon/uon.hpp>
#include <mongo/client/dbclient.h>
#include <cstdlib>
#include <boost/filesystem.hpp>
#include <fstream>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

extern uon::Value consolidate(std::map<std::string, uon::Value> reports);
extern void notify(uon::Value report);

static std::string collection = std::string("timberyard.reports");

int main(int argc, char** argv)
{
	// get home
	const char* homedir_c = std::getenv("HOME");

	if(homedir_c == NULL)
	{
		std::cerr << "Could not get HOME..." << std::endl;
		return 1;
	}

	boost::filesystem::path homedir(homedir_c);

	// lock
	std::cerr << "Getting file lock..." << std::endl;

	auto lock_file_path = homedir / ".birch-run-lock";
	std::ofstream lock_file(lock_file_path.string());

	if(!lock_file)
	{
		std::cerr << "Could not ensure existence of lock file..." << std::endl;
		return 1;
	}

	boost::interprocess::file_lock file_lock(lock_file_path.c_str());
	boost::interprocess::scoped_lock<boost::interprocess::file_lock> lock(file_lock);

	std::cerr << "Lock acquired..." << std::endl;

	// create database connection
	std::cerr << "Connect to database..." << std::endl;

	mongo::client::initialize();
	mongo::DBClientConnection db(true);

	try
	{
		std::cerr << "Connecting to database..." << std::endl;
		db.connect("localhost");
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error while connecting to database: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error while connecting to database: unknown" << std::endl;
		return 1;
	}

	try
	{
		std::cerr << "Check command line..." << std::endl;

		// read parameter
		boost::program_options::variables_map vm;
		std::string argNode, argInput;

		{
			boost::program_options::options_description optdescr;

			optdescr.add_options()
				("letterbox,l", "letterbox mode")
				("node,l", boost::program_options::value<std::string>(&argNode), "node name (letterbox mode)")
				("input,l", boost::program_options::value<std::string>(&argInput), "input directory (letterbox mode)")
				("help,h", "show this text")
				;

			boost::program_options::store( boost::program_options::command_line_parser(argc, argv).options(optdescr).run(), vm);
			boost::program_options::notify(vm);

			if(vm.count("help") > 0)
			{
				std::cout << optdescr << "\n";
				return 0;
			}
		}

		// check if we are in letterbox mode
		if(vm.count("letterbox") > 0)
		{
			std::cerr << "Handling letterbox mode..." << std::endl;

			if(argInput.empty())
			{
				argInput = ".";
			}

			boost::filesystem::path input = boost::filesystem::absolute(argInput);

			// read report
			uon::Value report = uon::read_json(input / "reports" / "oak.json" );

			// verify result
			if(!argNode.empty())
			{
				if(report.get("meta.system.name").to_string() != argNode)
				{
					throw std::runtime_error("invalid system name in report");
				}

				// maybe more stuff to validate...
			}

			// add files to report
			/*
			for(boost::filesystem::recursive_directory_iterator file(input), end; file != end; ++file)
			{
				if(boost::filesystem::is_regular_file(file->path()))
				{
					auto pref = input.string();
					auto path = file->path().string();

					if(path.find(pref) == 0)
					{
						path = path.substr(pref.length());

						while(path.length() > 0 && (path[0] == '/' || path[0] == '\\'))
						{
							path = path.substr(1);
						}
					}

					report.set( { "files", path }, file->path().string() );
				}
			}
			*/

			// insert report
			std::cout << "creating mongo query and op..." << std::endl;

			uon::Value query;
			query.set("repository", report.get("meta.repository").to_string());
			query.set("branch", report.get("meta.branch").to_string());
			query.set("commit", report.get("meta.commit.id.long").to_string());

			uon::Value op;
			report.escape_mongo();
			op.set( std::vector<std::string>{"$set", "hosts."+uon::escape_mongo_key(report.get("meta.arch.host.descriptor").to_string())}, report );
			op.set( std::vector<std::string>{"$addToSet", "tasks.consolidation"}, report.get("meta.id").to_string() );
			op.set( std::vector<std::string>{"$addToSet", "tasks.notification"}, report.get("meta.id").to_string() );
			op.set( std::vector<std::string>{"$set", "tasks.notification_timeout"}, (std::uint64_t)(std::time(nullptr) + (60 * 5)) );	// 5 minute timeout

			auto mquery = uon::to_mongo_bson(query);
			auto mop = uon::to_mongo_bson(op);

			std::cout << "running mongo query..." << std::endl;

			db.update(collection, mquery, mop, true, false);
		}
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error while processing letter in letterbox mode: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error while processing letter in letterbox mode: unknown" << std::endl;
		return 1;
	}

    // run consolidation process
	try
	{
		std::cerr << "Running consolidation process..." << std::endl;

		uon::Value query;
		query.set( std::vector<std::string>{"tasks.consolidation.0", "$exists"}, true );

		auto reports = db.query(collection, uon::to_mongo_bson(query));

		while(reports->more())
		{
			auto report = uon::from_mongo_bson(reports->nextSafe());
			report.unescape_mongo();

			std::cerr << "Handling report " << report.get("_id").to_string() << std::endl;

			auto consolidated = consolidate(report.get("hosts").to_object());

			std::cout << "creating mongo query and op..." << std::endl;

			query = uon::null;
			query.set("_id", report.get("_id"));

			uon::Value obj;
			consolidated.escape_mongo();
			obj.set( std::vector<std::string>{"$set", "consolidated"}, consolidated );
			obj.set( std::vector<std::string>{"$pull", "tasks.consolidation", "$in"}, report.get("tasks.consolidation").to_array() );

			auto mquery = uon::to_mongo_bson(query);
			auto mobj = uon::to_mongo_bson(obj);

			std::cout << "running mongo query..." << std::endl;

			db.update(collection, mquery, mobj);
		}
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error while running consolidation process: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error while running consolidation process: unknown" << std::endl;
		return 1;
	}

    // run notification process
	try
	{
		std::cerr << "Running notification process..." << std::endl;

		uon::Value query;
		query.set( std::vector<std::string>{"tasks.notification.0", "$exists"}, true );
		query.set( std::vector<std::string>{"tasks.consolidation.0", "$exists"}, false );
		query.set( std::vector<std::string>{"tasks.notification_timeout", "$lte"}, (std::uint64_t)std::time(nullptr) );

		auto reports = db.query(collection, uon::to_mongo_bson(query));

		while(reports->more())
		{
			auto report = uon::from_mongo_bson(reports->nextSafe());
			report.unescape_mongo();

			std::cerr << "Handling report " << report.get("_id").to_string() << std::endl;

			notify(report.get("consolidated"));

			std::cout << "creating mongo query and op..." << std::endl;

			query = uon::null;
			query.set("_id", report.get("_id"));

			uon::Value obj;
			obj.set({"$pull", "tasks.notification", "$in"}, report.get("tasks.notification").to_array());

			auto mquery = uon::to_mongo_bson(query);
			auto mobj = uon::to_mongo_bson(obj);

			std::cout << "running mongo query..." << std::endl;

			db.update(collection, mquery, mobj);
		}
	}
	catch ( const std::exception& e )
	{
		std::cerr << "Error while running notification process: " << e.what() << std::endl;
		return 1;
	}
	catch ( ... )
	{
		std::cerr << "Error while running notification process: unknown" << std::endl;
		return 1;
	}

    return 0;
}
