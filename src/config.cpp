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

void Config::apply( Priority priority, std::vector<std::string> variables )
{
	uon::Value snippet = uon::Object();

	for(auto var : variables)
	{
		auto i = var.find("=");

		if(i == std::string::npos)
		{
			throw std::runtime_error(std::string("could not apply variable: ") + var);
		}

		auto path = var.substr( 0, i );
		uon::Value value = var.substr( i+1 );

		snippet.merge(path, value);
	}

	apply( priority, snippet );
}

void Config::apply( Priority priority, std::map<std::string, uon::Value> variables )
{
	uon::Value snippet = uon::Object();

	for(auto var : variables)
	{
		snippet.merge(var.first, var.second);
	}

	apply( priority, snippet );
}

void Config::apply( Priority priority, std::string json )
{
	std::istringstream stream( json );
	stream.exceptions( std::istringstream::failbit | std::istringstream::badbit );

	apply(priority, stream);
}

void Config::apply( Priority priority, boost::filesystem::path json )
{
	std::ifstream stream;
	stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	stream.open( json.string() );

	apply(priority, stream);
}

void Config::apply( Priority priority, std::istream& json )
{
	json.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	uon::Value snippet = uon::read_json( json );

	apply( priority, snippet );
}

void Config::apply( Priority priority, std::string path, uon::Value value )
{
	uon::Value snippet = uon::Object();
	snippet.merge(path, value);

	apply( priority, snippet );
}

void Config::apply( Priority priority, uon::Value config )
{
	_snippets[priority].push_back(config);
	merge();
}

void Config::merge()
{
	// combine snippets
	_unresolved = uon::Object();

	for( auto priority : std::vector<Priority> {
			Priority::Base,
			Priority::Variant,
			Priority::Project,
			Priority::System,
			Priority::Environment,
			Priority::Arguments,
			Priority::Computed
		} )
	{
		for( auto snippet : _snippets[priority] )
		{
			_unresolved.merge(snippet);
		}
	}

	// resolve variables
	_resolved = _unresolved;

	std::function<uon::Value(std::string)> resolve = [&resolve, this](std::string value) -> uon::Value
	{
		auto i = value.find("${");

		if(i == std::string::npos)
		{
			return uon::Value(value);
		}

		auto j = value.find("}", i);

		if( i == 0 && j == (value.length() - 1) )
		{
			auto result = _resolved.get( value.substr(2, value.length()-3), uon::null );

			if(result.is_string())
			{
				result = resolve(result.as_string());
			}

			return result;
		}

		return resolve( value.substr(0, i) + _resolved.get( value.substr(i+2, j-i-2), uon::null ).to_string() + value.substr(j+1) );
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
