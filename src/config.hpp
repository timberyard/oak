#pragma once

#include <map>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <uon/uon.hpp>

namespace config {

	namespace builtin {

		extern const std::string base;
		extern const std::map<std::string, std::string> variants;

	}  // namespace: builtin

	class Config
	{
	public:
		enum class Priority {
			Base,
			Variant,
			Project,
			System,
			Environment,
			Arguments,
			Computed
		};

		void apply( Priority priority, std::vector<std::string> variables );
		void apply( Priority priority, std::map<std::string, uon::Value> variables );

		void apply( Priority priority, std::string json );
		void apply( Priority priority, boost::filesystem::path json );
		void apply( Priority priority, std::istream& json );

		void apply( Priority priority, std::string path, uon::Value value );
		void apply( Priority priority, uon::Value config );

		uon::Value get(std::string path) const;
		uon::Value get(std::vector<std::string> path) const;

		uon::Value get(std::string path, const uon::Value& defaultValue) const;
		uon::Value get(std::vector<std::string> path, const uon::Value& defaultValue) const;

		uon::Value unresolved();
		uon::Value resolved();

		Config();
		Config(const Config& other);

		Config& operator=(const Config& other);

	protected:
		void merge();

	protected:
		std::map< Priority, std::vector<uon::Value> > _snippets;
		uon::Value _unresolved;
		uon::Value _resolved;
	};

} // namespace: config
