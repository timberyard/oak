#include "config.hpp"
#include "ptree_utils.hpp"

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


std::string ConfigNode::value(std::string path)
{
	return resolve(_config.get<std::string>(path));
}

std::string ConfigNode::value()
{
	return resolve(_config.data());
}

ConfigNode ConfigNode::node(std::string path)
{
	return ConfigNode(_base, _config.get_child(path));
}

std::map<std::string, ConfigNode> ConfigNode::children()
{
	std::map<std::string, ConfigNode> result;

	for(auto i : _config)
	{
		result[i.first] = ConfigNode(_base, i.second);
	}

	return result;
}

std::string ConfigNode::resolve(std::string value)
{
	auto i = value.find("${");

	if(i == std::string::npos)
	{
		return value;
	}

	auto j = value.find("}", i);

	return resolve( value.substr(0, i) + _base.get<std::string>( value.substr( i+2, j-i-2 ) ) + value.substr(j+1) );
}

void ConfigNode::print(std::ostream& stream, bool resolved)
{
	boost::property_tree::ptree config = _config;

	if(resolved)
	{
		ptree_utils::traverse(
			config,
			[this] (boost::property_tree::ptree &parent, const boost::property_tree::ptree::path_type &childPath, boost::property_tree::ptree &child) {
				child.data() = resolve(child.data());
			});
	}

	boost::property_tree::write_json(stream, config);
}

json_spirit::Object ConfigNode::toSpirit(bool resolved)
{
	std::function<json_spirit::Object(boost::property_tree::ptree)> traverse
		= [this, resolved, &traverse] (boost::property_tree::ptree parent) -> json_spirit::Object
	{
		json_spirit::Object result;

		for(auto i : parent)
		{
			if(i.second.empty())
			{
				if(i.second.data().length() == 0)
				{
					result.push_back(json_spirit::Pair(i.first, json_spirit::Value()));
				}
				else
				{
					if(resolved)
					{
						result.push_back(json_spirit::Pair(i.first, resolve(i.second.data())));
					}
					else
					{
						result.push_back(json_spirit::Pair(i.first, i.second.data()));
					}
				}
			}
			else
			{
				result.push_back(json_spirit::Pair(i.first, traverse(i.second)));
			}
		}

		return result;
	};

	return traverse(_config);
}

ConfigNode::ConfigNode()
{
}

ConfigNode::ConfigNode(const ConfigNode& other)
	: _base(other._base), _config(other._config)
{
}

ConfigNode& ConfigNode::operator=(const ConfigNode& other)
{
	_base = other._base;
	_config = other._config;
	return *this;
}

ConfigNode::ConfigNode(boost::property_tree::ptree base, boost::property_tree::ptree config)
	: _base(base), _config(config)
{
}


Config::Config()
{ }

void Config::apply( Priority priority, std::string path, std::string value )
{
	boost::property_tree::ptree conftree;
	conftree.put<std::string>(path, value);

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::vector<std::string> config )
{
	boost::property_tree::ptree conftree;

	for(auto c : config)
	{
		auto i = c.find("=");

		if(i == std::string::npos)
		{
			throw std::runtime_error(std::string("could not apply config: ") + c);
		}

		auto path = c.substr( 0, i );
		auto value = c.substr( i+1 );

		conftree.put<std::string>(path, value);
	}

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::map<std::string, std::string> config )
{
	boost::property_tree::ptree conftree;

	for(auto c : config)
	{
		conftree.put<std::string>(c.first, c.second);
	}

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::string json )
{
	std::istringstream stream( json );
	stream.exceptions( std::ifstream::failbit | std::ifstream::badbit );

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
	boost::property_tree::ptree conftree;

	jsonfile.exceptions( std::ifstream::failbit | std::ifstream::badbit );
	boost::property_tree::read_json( jsonfile, conftree );

	apply( priority, conftree );
}

void Config::apply( Priority priority, std::string prefixPath, ConfigNode subtree )
{
	boost::property_tree::ptree conftree;
	conftree.put_child(prefixPath, subtree._config);

	apply( priority, conftree );
}

void Config::apply( Priority priority, boost::property_tree::ptree config )
{
	_configs[priority].push_back(config);
	merge();
}

void Config::merge()
{
	_config = boost::property_tree::ptree();

	for( auto p : std::vector<Priority> {
			Priority::Base, Priority::System, Priority::Variant, Priority::Project,
			Priority::Environment, Priority::Arguments, Priority::Computed
		} )
	{
		for( auto c : _configs[p] )
		{
			ptree_utils::merge(_config, c);
		}
	}

	_base = _config;
}

} // namespace: config
