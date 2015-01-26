#pragma once

#include <map>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/property_tree/json_parser.hpp>

namespace config {

	namespace builtin {

		extern const std::string base;

		namespace variants {

			extern const std::map<std::string, std::string> checkout;
			extern const std::map<std::string, std::string> integrate;
			extern const std::map<std::string, std::string> publish;

		} // namespace: variants

	}  // namespace: builtin

	class Config;

	class ConfigNode
	{
		friend class Config;

	public:
		std::string value(std::string path);
		std::string value();
		ConfigNode node(std::string path);
		std::map<std::string, ConfigNode> children();

		ConfigNode();
		ConfigNode(const ConfigNode& other);

		ConfigNode& operator=(const ConfigNode& other);

		void print(std::ostream& stream, bool resolved = true);

	protected:
		ConfigNode(boost::property_tree::ptree base, boost::property_tree::ptree config);

		std::string resolve(std::string value);

	protected:
		boost::property_tree::ptree _base;
		boost::property_tree::ptree _config;
	};

	class Config : public ConfigNode
	{
	public:
		enum class Priority {
			Base,
			System,
			Variant,
			Project,
			Environment,
			Arguments,
			Computed
		};

		void apply( Priority priority, std::string path, std::string value );
		void apply( Priority priority, std::vector<std::string> config );
		void apply( Priority priority, std::map<std::string, std::string> config );
		void apply( Priority priority, std::string json );
		void apply( Priority priority, boost::filesystem::path jsonfile );
		void apply( Priority priority, std::istream& jsonfile );
		void apply( Priority priority, std::string prefixPath, ConfigNode subtree );
		void apply( Priority priority, boost::property_tree::ptree config );

		Config();
		Config(const Config& other);

		Config& operator=(const Config& other);

	protected:
		void merge();

	protected:
		std::map< Priority, std::vector<boost::property_tree::ptree> > _configs;
	};

} // namespace: config
