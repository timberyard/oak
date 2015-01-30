#include "uon.hpp"

#include <set>
#include <boost/algorithm/string.hpp>

namespace uon {

Value Value::get(std::vector<std::string> path, Value defaultValue) const
{
	try
	{
		return getref(path);
	}
	catch(NotFound)
	{
		return defaultValue;
	}
}

Value& Value::getref(std::vector<std::string> path)
{
	return const_cast<Value&>(static_cast<const Value*>(this)->getref(path));
}

const Value& Value::getref(std::vector<std::string> path) const
{
	if(path.size() == 0)
	{
		return *this;
	}

	if(_value.type() == typeid(Object))
	{
		auto& object = boost::get<Object>(_value);
		auto i = object.find(path[0]);

		if(i != object.end())
		{
			return i->second.getref(std::vector<std::string>(path.begin()+1, path.end()));
		}
	}
	else
	if(_value.type() == typeid(Array))
	{
		auto& array = boost::get<Array>(_value);
		std::size_t i = std::stoul(path[0]);

		if(i < array.size())
		{
			return array[i].getref(std::vector<std::string>(path.begin()+1, path.end()));
		}
	}

	throw NotFound();
}

Value Value::get(std::string path, Value defaultValue) const
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return get(parts, defaultValue);
}

Value& Value::getref(std::string path)
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return getref(parts);
}

const Value& Value::getref(std::string path) const
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return getref(parts);
}

void Value::set(std::vector<std::string> path, Value value)
{
	if(path.size() == 0)
	{
		_value = value._value;
	}
	else
	{
		if(_value.type() != typeid(Object))
		{
			_value = Object();
		}

		auto& object = boost::get<Object>(_value);
		object[path.front()].set(std::vector<std::string>(path.begin()+1, path.end()), value);
	}
}

void Value::set(std::string path, Value value)
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	set(parts, value);
}

void Value::merge(Value overlay)
{
	if(_value.type() == typeid(Object) && overlay._value.type() == typeid(Object))
	{
		auto& this_object = boost::get<Object>(_value);
		auto& overlay_object = boost::get<Object>(overlay._value);

		for(auto i : overlay_object)
		{
			this_object[i.first].merge(i.second);
		}
	}
	else
	if(_value.type() == typeid(Array) && overlay._value.type() == typeid(Array))
	{
		auto& this_array = boost::get<Array>(_value);
		auto& overlay_array = boost::get<Array>(overlay._value);

		this_array.insert(this_array.end(), overlay_array.begin(), overlay_array.end());
	}
	else
	{
		_value = overlay._value;
	}
}

void Value::distinct()
{
	if(_value.type() == typeid(Array))
	{
		auto& array = boost::get<Array>(_value);

		std::set<Value> exists;

		for(auto i = array.begin(); i != array.end(); )
		{
			if(exists.find(*i) != exists.end())
			{
				i = array.erase(i);
			}
			else
			{
				exists.insert(*i);
				++i;
			}
		}
	}
}

}
