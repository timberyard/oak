#include "uon.hpp"

#include <sstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>

namespace uon
{

NotFound::NotFound(std::string path)
	: std::range_error(path + " not found")
{
}

NotFound::NotFound(std::vector<std::string> path)
	: NotFound(boost::algorithm::join(path, "."))
{
}

bool Null::operator<(const Null& other) const
{
	return false;
}

const Null null = Null();

Type Value::type() const
{
	if(_value.type() == typeid(Null))
		return Type::null;

	if(_value.type() == typeid(String))
		return Type::string;

	if(_value.type() == typeid(Number))
		return Type::number;

	if(_value.type() == typeid(Boolean))
		return Type::boolean;

	if(_value.type() == typeid(Object))
		return Type::object;

	if(_value.type() == typeid(Array))
		return Type::array;

	throw std::runtime_error("invalid type");
}

bool Value::is_null() const
{
	return _value.type() == typeid(Null);
}

bool Value::is_string() const
{
	return _value.type() == typeid(String);
}

bool Value::is_number() const
{
	return _value.type() == typeid(Number);
}

bool Value::is_boolean() const
{
	return _value.type() == typeid(Boolean);
}

bool Value::is_object() const
{
	return _value.type() == typeid(Object);
}

bool Value::is_array() const
{
	return _value.type() == typeid(Array);
}

String Value::as_string() const
{
	return boost::get<String>(_value);
}

Number Value::as_number() const
{
	return boost::get<Number>(_value);
}

Boolean Value::as_boolean() const
{
	return boost::get<Boolean>(_value);
}

Object Value::as_object() const
{
	return boost::get<Object>(_value);
}

Array Value::as_array() const
{
	return boost::get<Array>(_value);
}

String Value::to_string() const
{
	if(_value.type() == typeid(Null))
	{
		return "";
	}

	if(_value.type() == typeid(String))
	{
		return boost::get<String>(_value);
	}

	if(_value.type() == typeid(Number))
	{
		auto number = boost::get<Number>(_value);

		std::ostringstream string;
		string.setf ( std::ios::floatfield, std::ios::fixed );

		long double tmp;
		if(std::modf(number, &tmp) == 0.0)
		{
			string << std::setprecision(0);
		}
		else
		{
			string << std::setprecision(6);
		}

		string << number;
		return string.str();
	}

	if(_value.type() == typeid(Boolean))
	{
		return boost::get<Boolean>(_value) ? "true" : "false";
	}

	if(_value.type() == typeid(Object))
	{
		return write_json(*this, true);
	}

	if(_value.type() == typeid(Array))
	{
		return write_json(*this, true);
	}

	throw std::runtime_error("invalid type");
}

Number Value::to_number() const
{
	if(_value.type() == typeid(Null))
	{
		return 0.0;
	}

	if(_value.type() == typeid(String))
	{
		return std::stold(boost::get<String>(_value));
	}

	if(_value.type() == typeid(Number))
	{
		return boost::get<Number>(_value);
	}

	if(_value.type() == typeid(Boolean))
	{
		return boost::get<Boolean>(_value) ? 1.0 : 0.0;
	}

	if(_value.type() == typeid(Object))
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	if(_value.type() == typeid(Array))
	{
		return std::numeric_limits<double>::quiet_NaN();
	}

	throw std::runtime_error("invalid type");
}

Boolean Value::to_boolean() const
{
	if(_value.type() == typeid(Null))
	{
		return false;
	}

	if(_value.type() == typeid(String))
	{
		auto str = boost::get<String>(_value);
		boost::algorithm::to_lower(str);
		return (str == "true" || str == "yes" || str == "on" || str == "enabled" || str == "1") ? true : false;
	}

	if(_value.type() == typeid(Number))
	{
		return boost::get<Number>(_value) > 0 ? true : false;
	}

	if(_value.type() == typeid(Boolean))
	{
		return boost::get<Boolean>(_value);
	}

	if(_value.type() == typeid(Object))
	{
		return true;
	}

	if(_value.type() == typeid(Array))
	{
		return true;
	}

	throw std::runtime_error("invalid type");
}

Object Value::to_object() const
{
	if(_value.type() == typeid(Null))
	{
		return Object();
	}

	if(_value.type() == typeid(String))
	{
		return Object{std::pair<String, Value>{"", *this}};
	}

	if(_value.type() == typeid(Number))
	{
		return Object{std::pair<String, Value>{"", *this}};
	}

	if(_value.type() == typeid(Boolean))
	{
		return Object{std::pair<String, Value>{"", *this}};
	}

	if(_value.type() == typeid(Object))
	{
		return boost::get<Object>(_value);
	}

	if(_value.type() == typeid(Array))
	{
		Object obj;
		std::size_t i = 0;

		for(auto& e : boost::get<Array>(_value))
		{
			obj[std::to_string(i)] = e;
			i += 1;
		}

		return obj;
	}

	throw std::runtime_error("invalid type");
}

Array Value::to_array() const
{
	if(_value.type() == typeid(Null))
	{
		return Array();
	}

	if(_value.type() == typeid(String))
	{
		return Array{*this};
	}

	if(_value.type() == typeid(Number))
	{
		return Array{*this};
	}

	if(_value.type() == typeid(Boolean))
	{
		return Array{*this};
	}

	if(_value.type() == typeid(Object))
	{
		Array arr;

		for(auto& e : boost::get<Object>(_value))
		{
			arr.push_back(e.second);
		}

		return arr;
	}

	if(_value.type() == typeid(Array))
	{
		return boost::get<Array>(_value);
	}

	throw std::runtime_error("invalid type");
}

Value& Value::operator=(const Value& other)
{
	_value = other._value;
	return *this;
}

Value& Value::operator=(const Null& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const String& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const Number& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const Boolean& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const Object& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const Array& value)
{
	_value = value;
	return *this;
}

Value& Value::operator=(const char* value)
{
	_value = std::string(value);
	return *this;
}

Value& Value::operator=(std::uint64_t value)
{
	_value = (Number)value;
	return *this;
}

Value& Value::operator=(std::int64_t value)
{
	_value = (Number)value;
	return *this;
}

Value::Value()
	: _value(null)
{
}

Value::Value(const Value& other)
	: _value(other._value)
{
}

Value::Value(const Null& value)
	: _value(value)
{
}

Value::Value(const String& value)
	: _value(value)
{
}

Value::Value(const Number& value)
	: _value(value)
{
}

Value::Value(const Boolean& value)
	: _value(value)
{
}

Value::Value(const Object& value)
	: _value(value)
{
}

Value::Value(const Array& value)
	: _value(value)
{
}

Value::Value(const char* value)
	: _value(std::string(value))
{
}

Value::Value(std::uint64_t value)
	: _value((Number)value)
{
}

Value::Value(std::int64_t value)
	: _value((Number)value)
{
}

bool Value::operator<(const Value& other) const
{
	return _value < other._value;
}

}
