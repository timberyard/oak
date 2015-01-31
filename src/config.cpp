#include "config.hpp"

#include <sstream>
#include <fstream>

#define BUILTIN_CONFIG(name) \
	extern unsigned char configs_builtin_##name##_json[]; \
	extern unsigned int configs_builtin_##name##_json_len;

BUILTIN_CONFIG(base)
BUILTIN_CONFIG(checkout_nothing)
BUILTIN_CONFIG(integrate_nothing)
BUILTIN_CONFIG(integrate_c__)
BUILTIN_CONFIG(publish_nothing)
BUILTIN_CONFIG(publish_remote_rsync_mongo)

namespace config {

namespace builtin {

	const std::string base { configs_builtin_base_json, configs_builtin_base_json + configs_builtin_base_json_len };

	namespace variants {

		const std::map<std::string, std::string> checkout =
			{
				std::pair<std::string, std::string> { "nothing", std::string(configs_builtin_checkout_nothing_json, configs_builtin_checkout_nothing_json + configs_builtin_checkout_nothing_json_len) }
			};

		const std::map<std::string, std::string> integrate =
			{
				std::pair<std::string, std::string> { "nothing", std::string(configs_builtin_integrate_nothing_json, configs_builtin_integrate_nothing_json + configs_builtin_integrate_nothing_json_len) },
				std::pair<std::string, std::string> { "c++", std::string(configs_builtin_integrate_c___json, configs_builtin_integrate_c___json + configs_builtin_integrate_c___json_len) }
			};

		const std::map<std::string, std::string> publish =
			{
				std::pair<std::string, std::string> { "nothing", std::string(configs_builtin_publish_nothing_json, configs_builtin_publish_nothing_json + configs_builtin_publish_nothing_json_len) },
				std::pair<std::string, std::string> { "remote-rsync+mongo", std::string(configs_builtin_publish_remote_rsync_mongo_json, configs_builtin_publish_remote_rsync_mongo_json + configs_builtin_publish_remote_rsync_mongo_json_len) }
			};

	} // namespace: variants

} // namespace: builtin

Config::Config()
{ }

void Config::apply( Priority priority, std::vector<std::string> config )
{
	uon::Value conftree;

	for(auto c : config)
	{
		auto i = c.find("=");

		if(i == std::string::npos)
		{
			throw std::runtime_error(std::string("could not apply config: ") + c);
		}

		auto path = c.substr( 0, i );
		uon::Value value = c.substr( i+1 );

		conftree.set(path, value);
	}

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::map<std::string, std::string> config )
{
	uon::Value conftree;

	for(auto c : config)
	{
		conftree.set(c.first, uon::Value(c.second));
	}

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::string json )
{
	std::istringstream stream( json );
	stream.exceptions( std::istringstream::failbit | std::istringstream::badbit );

	apply(priority, stream);
}

void Config::apply( Priority priority, boost::filesystem::path jsonfile )
{
	std::ifstream stream;
	stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	stream.open( jsonfile.string() );

	apply(priority, stream);
}

void Config::apply( Priority priority, std::istream& jsonfile )
{
	jsonfile.exceptions( std::ifstream::failbit | std::ifstream::badbit );

	uon::Value conftree = uon::read_json( jsonfile );
	apply( priority, conftree );
}

void Config::apply( Priority priority, std::string path, uon::Value value )
{
	uon::Value conftree;
	conftree.set(path, value);

	apply( priority, conftree );
}

void Config::apply( Priority priority, uon::Value config )
{
	_snippets[priority].push_back(config);
	merge();
}

void Config::merge()
{
	// combine snippets
	_unresolved = uon::Value();

	for( auto p : std::vector<Priority> {
			Priority::Base,
			Priority::Variant,
			Priority::Project,
			Priority::System,
			Priority::Environment,
			Priority::Arguments,
			Priority::Computed
		} )
	{
		for( auto snippet : _snippets[p] )
		{
			_unresolved.merge(snippet);
		}
	}

	// resolve variables
	_resolved = _unresolved;

	std::function<std::string(std::string)> resolve = [&resolve, this](std::string value) -> std::string
	{
		auto i = value.find("${");

		if(i == std::string::npos)
		{
			return value;
		}

		auto j = value.find("}", i);

		return resolve( value.substr(0, i) + _resolved.get( value.substr( i+2, j-i-2 ), uon::null ).to_string() + value.substr(j+1) );
	};

	_resolved.traverse([&resolve] (uon::Value& value, std::vector<std::string>)
	{
		if(value.is_string())
		{
			value = resolve(value.as_string());
		}
	});
}

uon::Value Config::get(std::string path) const
{
	return _resolved.get(path);
}

uon::Value Config::get(std::vector<std::string> path) const
{
	return _resolved.get(path);
}

uon::Value Config::get(std::string path, const uon::Value& defaultValue) const
{
	return _resolved.get(path, defaultValue);
}

uon::Value Config::get(std::vector<std::string> path, const uon::Value& defaultValue) const
{
	return _resolved.get(path, defaultValue);
}

uon::Value Config::unresolved()
{
	return _unresolved;
}

uon::Value Config::resolved()
{
	return _resolved;
}

} // namespace: config
