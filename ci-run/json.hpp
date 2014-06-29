#pragma once

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <type_traits>
#include <boost/variant.hpp>
#include <boost/preprocessor.hpp>

namespace Json
{
	// Exceptions
	namespace Exception
	{
		struct InvalidPath : public std::runtime_error
		{
			InvalidPath() : std::runtime_error("invalid path") { }
		};

		struct InvalidType : public std::runtime_error
		{
			InvalidType() : std::runtime_error("invalid type") { }
		};
	};

	// Primitive Types
	typedef int64_t Integer;
	typedef double Float;
	typedef std::string String;
	typedef bool Boolean;
	class Null { };
	class Undefined { };

	// Value Type
	typedef boost::make_recursive_variant<
			std::map<std::string, boost::recursive_variant_>,
			std::vector<boost::recursive_variant_>,
			Integer,
			Float,
			String,
			Boolean,
			Null,
			Undefined
		>::type Value;

	// Object, Vector Type
	typedef std::map<std::string, Value> Object;
	typedef std::vector<Value> Array;

	// Path Type
	typedef std::vector<std::string> Path;

	// Lookup Value by Path

	enum Mode
	{
		MODE_EXISTING = 1,
		MODE_CREATE = 2,
	};

	Value& lookup(Value& root, const Path& path, Mode mode = MODE_EXISTING);
	const Value& lookup(const Value& root, const Path& path);

	Value& create(Value& root, const Path& path);

	template<typename TargetValue>
	TargetValue& lookup(Value& root, const Path& path, Mode mode = MODE_EXISTING)
	{
		Value& value = lookup(root, path, mode);

		if(value.type() != typeid(typename std::remove_const<TargetValue>::type))
		{
			if(mode == MODE_EXISTING)
			{
				throw Exception::InvalidType();
			}
			else
			{
				value = TargetValue();
			}
		}

		return boost::get<typename std::remove_const<TargetValue>::type>(value);
	}

	template<typename TargetValue>
	const TargetValue& lookup(const Value& root, const Path& path)
	{
		const Value& value = lookup(root, path);

		if(value.type() != typeid(typename std::remove_const<TargetValue>::type))
		{
			throw Exception::InvalidType();
		}

		return boost::get<typename std::remove_const<TargetValue>::type>(value);
	}

	template<typename TargetValue>
	TargetValue& create(Value& root, const Path& path)
	{
		return lookup<TargetValue>(root, path, MODE_CREATE);
	}

	// Serialization
	enum Format
	{
		FORMAT_JSON = 1,
		FORMAT_SIMPLEXML = 2
	};

	Value read(std::istream& stream, Format format);
	void write(const Value& val, std::ostream& stream, Format format);
}

#define _JSON_PATH_STRINGIFY(r, data, elem) BOOST_PP_STRINGIZE(elem),
#define JSON_PATH(...) Json::Path{ BOOST_PP_LIST_FOR_EACH(_JSON_PATH_STRINGIFY, _, BOOST_PP_VARIADIC_TO_LIST(__VA_ARGS__)) }
