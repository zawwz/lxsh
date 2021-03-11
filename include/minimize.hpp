#ifndef MINIMIZE_HPP
#define MINIMIZE_HPP

#include "struc.hpp"

#include <regex>
#include <string>

void minimize_var(_obj* in, std::regex const& exclude);
void minimize_fct(_obj* in, std::regex const& exclude);

bool delete_unused_fct(_obj* in, std::regex const& exclude);
bool delete_unused_var(_obj* in, std::regex const& exclude);
void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude);

void minimize_quotes(_obj* in);

#endif //MINIMIZE_HPP
