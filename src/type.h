#pragma once

#include <string>

namespace rocket {

enum class Type { Invalid, Int, Float, Bool, Char, String, Unit };

Type typeFromName(const std::string& name);
const char* typeName(Type type);

} // namespace rocket
