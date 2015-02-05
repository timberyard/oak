#include <uon/uon.hpp>
#include <stdexcept>
#include <boost/algorithm/string/trim.hpp>

uon::Value consolidate(std::map<std::string, uon::Value> reports)
{
	if(reports.size() == 0)
	{
		throw std::invalid_argument("at least one report required");
	}

	uon::Value cs;

	// meta data
	cs.set("meta.project", reports.begin()->second.get("meta.project"));
	cs.set("meta.repository", reports.begin()->second.get("meta.repository"));
	cs.set("meta.branch", reports.begin()->second.get("meta.branch"));
	cs.set("meta.commit", reports.begin()->second.get("meta.commit"));

	// meta: archs
	uon::Object meta_archs;

	for(auto report : reports)
	{
		auto host_descr = report.second.get("meta.arch.host.descriptor").to_string();

		meta_archs[host_descr] = report.second.get("meta.arch");
	}

	cs.set("meta.archs", meta_archs);

	// meta: buildgap
	uon::Array meta_buildgap;

	for(auto report : reports)
	{
		auto buildgap = report.second.get("meta.buildgap").to_array();

		meta_buildgap.insert(meta_buildgap.end(), buildgap.begin(), buildgap.end());
	}

	uon::unique(meta_buildgap);

	cs.set("meta.buildgap", meta_buildgap);

	// tasks

	uon::Object tasks_cs;

	for(auto report : reports)
	{
		auto host_descr = report.second.get("meta.arch.host.descriptor").to_string();

		for(auto task : report.second.get("tasks").to_object())
		{
			uon::Value& task_cs = tasks_cs[task.first];

			// consolidate type
			auto type = task.second.get("type").to_string();

			if(task_cs.get("type", type).to_string() != type)
			{
				throw std::runtime_error("task type mismatch");
			}

			task_cs.set("type", type);

			// consolidate name
			auto name = task.second.get("name").to_string();

			if(task_cs.get("name", name).to_string() != name)
			{
				throw std::runtime_error("task name mismatch");
			}

			task_cs.set("name", name);

			// consolidate status
			auto status_cs = task_cs.get("status.consolidated", "ok").to_string();
			auto status_src = task.second.get("status").to_string();

			if(status_cs != "error" && status_cs != "warning" && status_cs != "ok")
			{
				status_cs = "error";
			}

			if(status_src != "error" && status_src != "warning" && status_src != "ok")
			{
				status_src = "error";
			}

			if(status_cs == "error" || status_src == "error")
			{
				status_cs = "error";
			}
			else
			if(status_cs == "warning" || status_src == "warning")
			{
				status_cs = "warning";
			}
			else
			{
				status_cs = "ok";
			}

			task_cs.set("status.consolidated", status_cs);
			task_cs.set({"status", host_descr}, status_src);

			// consolidate message
			{
				auto msg = boost::trim_copy(task.second.get("message").to_string());

				if(msg.length() > 0)
				{
					task_cs.set( {"message", host_descr}, msg );
				}
			}

			// consolidate warnings and errors
			task_cs.set( "warnings", (std::uint64_t)0 );
			task_cs.set( "errors", (std::uint64_t)0 );

			// consolidate details
			if(type == "build:cmake")
			{
				// consolidate results
				auto results_cs = task_cs.get("details", uon::Array()).to_array();

				for(auto result : task.second.get("details.results", uon::Array()).to_array())
				{
					auto entry_cs = results_cs.end();

					for(auto i = results_cs.begin(); i != results_cs.end(); ++i)
					{
						if(    i->get("type") == result.get("type")
							&& i->get("message") == result.get("message")
							&& i->get("filename") == result.get("filename")
							&& i->get("row") == result.get("row")
							// && i->get("column") == result.get("column")
						)
						{
							entry_cs = i;
							break;
						}
					}

					if(entry_cs == results_cs.end())
					{
						results_cs.push_back(result);
						entry_cs = results_cs.end() - 1;
					}

					auto hosts_cs = entry_cs->get("hosts", uon::Array()).to_array();

					if(std::find(hosts_cs.begin(), hosts_cs.end(), uon::Value(host_descr)) == hosts_cs.end())
					{
						hosts_cs.push_back(uon::Value(host_descr));
					}

					entry_cs->set("hosts", hosts_cs);
				}

				task_cs.set("details", results_cs);

				// consolidate warnings and errors
				std::size_t warnings_cs = 0, errors_cs = 0;

				for(auto i : results_cs)
				{
					if(i.get("type").to_string() == "error")
					{
						errors_cs += 1;
					}
					else
					if(i.get("type").to_string() == "warning")
					{
						warnings_cs += 1;
					}
				}

				task_cs.set( "warnings", warnings_cs );
				task_cs.set( "errors", errors_cs );
			}
			else
			if(type == "analysis:cppcheck")
			{
				// consolidate results
				auto results_cs = task_cs.get("details", uon::Array()).to_array();

				for(auto result : task.second.get("details.errors", uon::Array()).to_array())
				{
					auto entry_cs = results_cs.end();

					for(auto i = results_cs.begin(); i != results_cs.end(); ++i)
					{
						if(    i->get("type") == result.get("type")
							&& i->get("message") == result.get("message")
							&& i->get("severity") == result.get("severity")
							&& i->get("file") == result.get("file")
							&& i->get("line") == result.get("line")
						)
						{
							entry_cs = i;
							break;
						}
					}

					if(entry_cs == results_cs.end())
					{
						results_cs.push_back(result);
						entry_cs = results_cs.end() - 1;
					}

					auto hosts_cs = entry_cs->get("hosts", uon::Array()).to_array();

					if(std::find(hosts_cs.begin(), hosts_cs.end(), uon::Value(host_descr)) == hosts_cs.end())
					{
						hosts_cs.push_back(uon::Value(host_descr));
					}

					entry_cs->set("hosts", hosts_cs);
				}

				task_cs.set("details", results_cs);

				// consolidate warnings
				task_cs.set( "warnings", results_cs.size() );
			}
			else
			if(type == "test:googletest")
			{
				for(auto testsuite : task.second.get("details.tests", uon::Object()).to_object())
				{
					for(auto test : testsuite.second.to_object())
					{
						auto test_cs = task_cs.get({"details", testsuite.first, test.first}, uon::Object{
							{"name", test.second.get("name")},
							{"result", test.second.get("result")},
							{"status", test.second.get("status")},
							{"message", uon::Object()}
						});

						if(test.second.get("result") == "Error")
						{
							test_cs.set("result", "Error");
						}

						if(test_cs.get("status") != test.second.get("status"))
						{
							test_cs.set("status", "mixed");
						}

						auto msg = boost::trim_copy(test.second.get("message").to_string());

						if(msg.length() > 0)
						{
							test_cs.set( {"message", host_descr}, msg );
						}

						task_cs.set( {"details", testsuite.first, test.first}, test_cs );
					}
				}

				std::size_t errors = 0;

				for(auto testsuite : task_cs.get("details", uon::Object()).to_object())
				{
					for(auto test : testsuite.second.to_object())
					{
						if(test.second.get("result") == "Error")
						{
							errors += 1;
						}
					}
				}

				task_cs.set( "errors", (std::uint64_t)errors );
			}
		}
	}

	cs.set("tasks", tasks_cs);

	return cs;
}
