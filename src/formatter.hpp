#pragma once

#include <uon/uon.hpp>
#include <iostream>

namespace formatter {

extern void markdown(uon::Value input, std::ostream& output);

} // namespace: formatter
