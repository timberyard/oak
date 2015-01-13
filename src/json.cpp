#include "json.hpp"
#include "json.grammar.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>

namespace Json
{
	Value& lookup(Value& root, const Path& path, Mode mode)
	{
		Value* current = &root;

		for(const std::string& index : path)
		{
			if(current->type() != typeid(Object))
			{
				if(mode == MODE_EXISTING)
				{
					throw Exception::InvalidPath();
				}
				else
				{
					*current = Object();
				}
			}

			Object& object = boost::get<Object>(*current);

			if(mode == MODE_EXISTING)
			{
				auto i = object.find(index);

				if(i == object.end())
				{
					throw Exception::InvalidPath();
				}

				current = &(i->second);
			}
			else
			{
				current = &(object[index]);
			}
		}

		return *current;
	}

	const Value& lookup(const Value& root, const Path& path)
	{
		const Value* current = &root;

		for(const std::string& index : path)
		{
			if(current->type() != typeid(Object))
			{
				throw Exception::InvalidPath();
			}

			const Object& object = boost::get<Object>(*current);

			auto i = object.find(index);

			if(i == object.end())
			{
				throw Exception::InvalidPath();
			}

			current = &(i->second);
		}

		return *current;
	}

	Value& create(Value& root, const Path& path)
	{
		return lookup(root, path, MODE_CREATE);
	}

	Value read(std::istream& stream, Format format)
	{
		std::string input;

		stream.unsetf(std::ios::skipws);
		std::copy(
	        std::istream_iterator<char>(stream),
	        std::istream_iterator<char>(),
	        std::back_inserter(input));

		JsonGrammar<std::string::const_iterator> grammar;
		Value value;

		std::string::const_iterator iter = input.begin();
		std::string::const_iterator end = input.end();

		bool result = boost::spirit::qi::phrase_parse(iter, end, grammar, boost::spirit::ascii::space, value);

		return value;
	}

	std::string encodeXmlString(const std::string& data)
	{
		std::string result;
		result.reserve(data.size());

		for(size_t pos = 0; pos < data.size(); ++pos)
		{
			switch(data[pos])
			{
				case '&':  result.append("&amp;");       break;
				case '<':  result.append("&lt;");        break;
				case '>':  result.append("&gt;");        break;
				case '\"': result.append("&quot;");      break;
				case '\'': result.append("&apos;");      break;
				case '\b': result.append("#8;");         break;
				case '\t': result.append("#09;");        break;
				case '\f': result.append("#0c;");        break;
				case '\n': result.append("#10;");        break;
				case '\r': result.append("#13;");        break;
				default:   result.append(&data[pos], 1); break;
			}
		}

		return result;
	}

	std::string encodeJsonString(const std::string& data)
	{
		std::string result;
		result.reserve(data.size());

		for(size_t pos = 0; pos < data.size(); ++pos)
		{
			switch(data[pos])
			{
				case '\"': result.append("\\\"");        break;
				case '\\': result.append("\\\\");        break;
				case '/':  result.append("\\/");         break;
				case '\b': result.append("\\b");         break;
				case '\f': result.append("\\f");         break;
				case '\n': result.append("\\n");         break;
				case '\r': result.append("\\r");         break;
				case '\t': result.append("\\t");         break;
				default:   result.append(&data[pos], 1); break;
			}
		}

		return result;
	}

	std::string deriveXmlElement(const std::string& data)
	{
		std::string result;
		result.reserve(data.size()+1);

		for(size_t pos = 0; pos < data.size(); ++pos)
		{
			if((data[pos] >= '0' && data[pos] <= '9') || data[pos] == '.' || data[pos] == '-')
			{
				if(result.size() == 0) { result.append("_"); }
				result.append(&data[pos], 1);
			}
			else
			if((data[pos] >= 'a' && data[pos] <= 'z') || (data[pos] >= 'A' && data[pos] <= 'Z') || data[pos] == ':' || data[pos] == '_')
			{
				result.append(&data[pos], 1);
			}
		}

		return result;
	}

	void writeXml(const Value& value, std::ostream& stream, std::string currentIndex, unsigned int level)
	{
		std::string levelString(level, '\t');

		currentIndex = deriveXmlElement(currentIndex);

		stream << std::endl << levelString << "<" << currentIndex << ">";

		if(value.type() == typeid(Integer))
		{
			stream << boost::get<Integer>(value);
		}
		else
		if(value.type() == typeid(Float))
		{
			stream << boost::get<Float>(value);
		}
		else
		if(value.type() == typeid(String))
		{
			stream << encodeXmlString(boost::get<String>(value));
		}
		else
		if(value.type() == typeid(Boolean))
		{
			stream << (boost::get<Boolean>(value) ? "true" : "false");
		}
		else
		if(value.type() == typeid(Null))
		{
			stream << "null";
		}
		else
		if(value.type() == typeid(Object))
		{	
			for (auto& entry : boost::get<Object>(value))
			{
				writeXml(entry.second, stream, entry.first, level + 1);
			}

			stream << std::endl << levelString;
		}
		else
		if(value.type() == typeid(Array))
		{
			int i = 0;

			for (auto& entry : boost::get<Array>(value))
			{
				writeXml(entry, stream, (boost::format("%1%") % i++).str(), level + 1);
			}

			stream << std::endl << levelString;
		}
		else
		{
			throw Exception::InvalidType();
		}

		stream << "</" << currentIndex << ">";
	}

	void writeJson(const Value& value, std::ostream& stream, unsigned int level)
	{
		std::string levelString(level, '\t');

		if(value.type() == typeid(Integer))
		{
			stream << boost::get<Integer>(value);
		}
		else
		if(value.type() == typeid(Float))
		{
			stream << boost::get<Float>(value);
		}
		else
		if(value.type() == typeid(String))
		{
			stream << "\"" << encodeJsonString(boost::get<String>(value)) << "\"";
		}
		else
		if(value.type() == typeid(Boolean))
		{
			stream << (boost::get<Boolean>(value) ? "true" : "false");
		}
		else
		if(value.type() == typeid(Null))
		{
			stream << "null";
		}
		else
		if(value.type() == typeid(Object))
		{
			stream << std::endl << levelString << "{";

			bool first = true;

			for (auto& entry : boost::get<Object>(value))
			{
				if(first) { first = false; }
				else { stream << ","; }

				stream << std::endl << levelString << "\t";

				stream << "\"" << encodeJsonString(entry.first) << "\": ";
				writeJson(entry.second, stream, level + 1);
			}

			stream << std::endl << levelString << "}";
		}
		else
		if(value.type() == typeid(Array))
		{
			stream << std::endl << levelString << "[";

			bool first = true;

			for (auto& entry : boost::get<Array>(value))
			{
				if(first) { first = false; }
				else { stream << ","; }

				stream << std::endl << levelString << "\t";

				writeJson(entry, stream, level + 1);
			}

			stream << std::endl << levelString << "]";
		}
		else
		{
			throw Exception::InvalidType();
		}
	}

	void write(const Value& value, std::ostream& stream, Format format)
	{
		switch(format)
		{
			case FORMAT_JSON:
				writeJson(value, stream, 0);
				break;

			case FORMAT_XML:
				stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
				writeXml(value, stream, "document", 0);
				break;

			default:
				throw std::runtime_error("invalid format");
		}
	}

	void test()
	{
		Value object = Object {
			{"tasks", Object {
				{"build:main", Object {
					{"type", String("cmake")},
					{"result", Object {
						{"status", String("Error")},
						{"warnings", Integer(2)},
						{"errors", Integer(1)}
					}},
				}},
				{"test:unittest", Object { }},
			}}
		};

		create(object, JSON_PATH(tasks, test:unittest, type)) = String("googletest");
		create(object, JSON_PATH(tasks, test:unittest, result))
			= Object {
				{"status", String("Warning")},
				{"warnings", Integer(1)},
				{"errors", Integer(0)}
			};

		lookup(object, JSON_PATH(tasks, test:unittest, result, errors)) = Integer(5);

		write(object, std::cout, FORMAT_JSON);
		std::cout << std::endl << std::endl;

		write(object, std::cout, FORMAT_XML);
		std::cout << std::endl << std::endl;

		/*
		Path path = JSON_PATH(abc, xyz);

		for(std::string element : path)
			std::cout << "/" << element;

		std::cout << std::endl;


		Path path2 = JSON_PATH(abc, def);

		for(std::string element : path2)
			std::cout << "/" << element;

		std::cout << std::endl;


		Value object = Object {
			{"abc", Object {
				{"xyz", String("test")},
				{"qqq", Null()}
			}}
		};

		const Value& constObjectRef = object;

		std::cout << lookup<String>(object, path) << std::endl;
		std::cout << lookup<const String>(object, path) << std::endl;
		std::cout << boost::get<String>(lookup(object, path)) << std::endl;
		std::cout << boost::get<String>(lookup(constObjectRef, path)) << std::endl;

		lookup(object, path) = String("test2");

		std::cout << boost::get<String>(lookup(constObjectRef, path)) << std::endl;

		lookup(object, path) = Boolean(true);

		std::cout << boost::get<Boolean>(lookup(constObjectRef, path)) << std::endl;
		std::cout << lookup<const Boolean>(constObjectRef, path) << std::endl;

		lookup<Boolean>(object, path) = false;

		std::cout << lookup<const Boolean>(constObjectRef, path) << std::endl;

		create<String>(object, path2) = String("test2");

		std::cout << lookup<String>(object, path2) << std::endl;

		std::cout << std::endl << std::endl;

		write(object, std::cout, FORMAT_JSON);
		std::cout << std::endl << std::endl;

		lookup<Boolean>(object, path) = true;
		create<String>(object, Path{"abc", "124"}) = String("test2");
		create<String>(object, Path{"abc", "125"}) = String("tes't2");
		create<String>(object, Path{"abc", "126"}) = String("tes\"t2");
		create<Array>(object, JSON_PATH(123, 456)) = Array { String("1"), String("2"), Integer(3) };

		write(object, std::cout, FORMAT_JSON);
		std::cout << std::endl << std::endl;

		write(String("abc"), std::cout, FORMAT_JSON);
		std::cout << std::endl << std::endl;

		write(object, std::cout, FORMAT_XML);
		std::cout << std::endl << std::endl;
		*/
	}
}
