#pragma once

#include <cstddef>
#include <string>
#include <map>
#include <deque>
#include <vector>
#include <boost/variant.hpp>

namespace uon
{
	class NotFound : public std::exception { };

	enum class Type { null, string, number, boolean, object, array };

	struct Null
	{
		bool operator<(const Null& other) const;
	};

	extern const Null null;

	using String = std::string;
	using Number = long double;
	using Boolean = bool;

	class Value;

	using Object = std::map<String, Value>;
	using Array = std::deque<Value>;

	class Value
	{
	public:
		Type type() const;

		bool is_null() const;
		bool is_string() const;
		bool is_number() const;
		bool is_boolean() const;
		bool is_object() const;
		bool is_array() const;

		String as_string() const;
		Number as_number() const;
		Boolean as_boolean() const;
		Object as_object() const;
		Array as_array() const;

		String to_string() const;
		Number to_number() const;
		Boolean to_boolean() const;
		Object to_object() const;
		Array to_array() const;

		Value& operator=(const Value& other);
		Value& operator=(const Null& value);
		Value& operator=(const String& value);
		Value& operator=(const Number& value);
		Value& operator=(const Boolean& value);
		Value& operator=(const Object& value);
		Value& operator=(const Array& value);

		Value();
		Value(const Value& other);
		Value(const Null& value);
		Value(const String& value);
		Value(const Number& value);
		Value(const Boolean& value);
		Value(const Object& value);
		Value(const Array& value);

		Value copy() const;

		bool operator<(const Value& other) const;

	public:
		Value get(std::string path) const;
		Value get(std::vector<std::string> path) const;

		Value get(std::string path, const Value& defaultValue) const;
		Value get(std::vector<std::string> path, const Value& defaultValue) const;

		Value& getref(std::string path);
		Value& getref(std::vector<std::string> path);

		const Value& getref(std::string path) const;
		const Value& getref(std::vector<std::string> path) const;

		void set(std::string path, const Value& subject);
		void set(std::vector<std::string> path, const Value& subject);

		void merge(const Value& overlay);
		void merge(std::string path, const Value& overlay);
		void merge(std::vector<std::string> path, const Value& overlay);

		void distinct();

		void traverse(std::function<void(Value&,std::vector<std::string>)> functor, std::vector<std::string> basepath = std::vector<std::string>{});

	private:
		boost::make_recursive_variant<
			Null,
			String,
			Number,
			Boolean,
			boost::recursive_wrapper<Object>,
			boost::recursive_wrapper<Array>
		>::type _value;
	};
};
