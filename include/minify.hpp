#ifndef MINIFY_HPP
#define MINIFY_HPP

#include "struc.hpp"
#include "processing.hpp"

#include <regex>
#include <string>

std::string gen_minmap(strmap_t const& map, std::string const& prefix);
void read_minmap(std::string const& filepath, strmap_t* varmap, strmap_t* fctmap);

bool r_replace_fct(_obj* in, strmap_t* fctmap);
bool r_replace_var(_obj* in, strmap_t* varmap);

strmap_t minify_var(_obj* in, std::regex const& exclude);
strmap_t minify_fct(_obj* in, std::regex const& exclude);

void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude);

void minify_generic(_obj* in);

#endif //MINIFY_HPP
