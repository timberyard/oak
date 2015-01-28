#pragma once

#include <boost/property_tree/ptree.hpp>
#include <iostream>

namespace formatter {

extern void markdown(boost::property_tree::ptree input, std::ostream& output);

} // namespace: formatter
