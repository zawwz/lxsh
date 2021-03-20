#ifndef MINIFY_HPP
#define MINIFY_HPP

#include "struc.hpp"

#include <regex>
#include <string>

void minify_var(_obj* in, std::regex const& exclude);
void minify_fct(_obj* in, std::regex const& exclude);

bool delete_unused_fct(_obj* in, std::regex const& exclude);
bool delete_unused_var(_obj* in, std::regex const& exclude);
void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude);

void minify_quotes(_obj* in);

#endif //MINIFY_HPP
