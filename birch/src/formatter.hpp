#pragma once

#include <uon/uon.hpp>
#include <iostream>

namespace formatter {

extern void html(uon::Value input, std::ostream& output);

} // namespace: formatter
