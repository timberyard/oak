#include "uon.hpp"

#include <set>
#include <boost/algorithm/string.hpp>

namespace uon {

Value Value::get(std::string path) const
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return get(parts);
}

Value Value::get(std::vector<std::string> path) const
{
	return getref(path);
}

Value Value::get(std::string path, const Value& defaultValue) const
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return get(parts, defaultValue);
}

Value Value::get(std::vector<std::string> path, const Value& defaultValue) const
{
	try
	{
		return getref(path);
	}
	catch(const NotFound&)
	{
		return defaultValue;
	}
}

Value& Value::getref(std::string path)
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return getref(parts);
}

Value& Value::getref(std::vector<std::string> path)
{
	return const_cast<Value&>(static_cast<const Value*>(this)->getref(path));
}

const Value& Value::getref(std::string path) const
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	return getref(parts);
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
			try
			{
				return i->second.getref(std::vector<std::string>(path.begin()+1, path.end()));
			}
			catch(const NotFound&)
			{
				throw NotFound(boost::algorithm::join(path, "."));
			}
		}
	}
	else
	if(_value.type() == typeid(Array))
	{
		auto& array = boost::get<Array>(_value);
		std::size_t i = std::stoul(path[0]);

		if(i < array.size())
		{
			try
			{
				return array[i].getref(std::vector<std::string>(path.begin()+1, path.end()));
			}
			catch(const NotFound&)
			{
				throw NotFound(boost::algorithm::join(path, "."));
			}
		}
	}

	throw NotFound(boost::algorithm::join(path, "."));
}

void Value::set(std::vector<std::string> path, const Value& value)
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

void Value::set(std::string path, const Value& value)
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	set(parts, value);
}

void Value::merge(const Value& overlay)
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

void Value::merge(std::string path, const Value& overlay)
{
	std::vector<std::string> parts;
	boost::split(parts, path, boost::is_any_of("."));
	merge(parts, overlay);
}

void Value::merge(std::vector<std::string> path, const Value& overlay)
{
	if(path.size() == 0)
	{
		merge(overlay);
	}
	else
	{
		if(_value.type() != typeid(Object))
		{
			_value = Object();
		}

		auto& object = boost::get<Object>(_value);
		object[path.front()].merge(std::vector<std::string>(path.begin()+1, path.end()), overlay);
	}
}

void Value::traverse(std::function<void(uon::Value&,std::vector<std::string>)> functor, std::vector<std::string> basepath)
{
	functor(*this, basepath);

	if(_value.type() == typeid(Object))
	{
		for(auto& i : boost::get<Object>(_value))
		{
			std::vector<std::string> path = basepath;
			path.push_back(i.first);

			i.second.traverse(functor, path);
		}
	}
	else
	if(_value.type() == typeid(Array))
	{
		std::size_t j = 0;

		for(auto& i : boost::get<Array>(_value))
		{
			std::vector<std::string> path = basepath;
			path.push_back(std::to_string(j++));

			i.traverse(functor, path);
		}
	}
}

void unique(Array& array)
{
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
