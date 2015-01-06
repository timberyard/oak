#pragma once

#include <boost/property_tree/ptree.hpp>

template<typename Method>
void ptree_traverse(boost::property_tree::ptree &parent, const boost::property_tree::ptree::path_type &childPath, boost::property_tree::ptree &child, Method method)
{
	method(parent, childPath, child);

	for(boost::property_tree::ptree::iterator it = child.begin(); it != child.end(); ++it)
	{
		boost::property_tree::ptree::path_type curPath = childPath / boost::property_tree::ptree::path_type(it->first);
		ptree_traverse(parent, curPath, it->second, method);
	}
}

template<typename Method>
void ptree_traverse(boost::property_tree::ptree &parent, Method method)
{
	ptree_traverse(parent, "", parent, method);
}

void ptree_merge_data(boost::property_tree::ptree &target, boost::property_tree::ptree &parent, const boost::property_tree::ptree::path_type &childPath, boost::property_tree::ptree &child)
{
	target.put(childPath, child.data());
}

void ptree_merge(boost::property_tree::ptree &target, boost::property_tree::ptree &source)
{  
	ptree_traverse(source, bind(&ptree_merge_data, ref(target), _1, _2, _3));
}
