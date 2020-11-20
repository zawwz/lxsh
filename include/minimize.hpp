#ifndef MINIMIZE_HPP
#define MINIMIZE_HPP

#include "struc.hpp"

#include <regex>
#include <string>

typedef std::map<std::string,uint32_t> countmap_t;

extern std::regex re_var_exclude;
extern std::regex re_fct_exclude;

extern const std::regex regex_null;

#define RESERVED_VARIABLES "HOME", "PATH", "SHELL", "PWD", "OPTIND", "OPTARG"

std::regex var_exclude_regex(std::string const& in);
std::regex fct_exclude_regex(std::string const& in);

void list_vars(_obj* in, std::regex const& exclude);
void list_fcts(_obj* in, std::regex const& exclude);
void list_cmds(_obj* in, std::regex const& exclude);

void minimize_var(_obj* in, std::regex const& exclude);
void minimize_fct(_obj* in, std::regex const& exclude);

void delete_unused_fct(_obj* in, std::regex const& exclude);
// void delete_unused_var(_obj* in, std::regex const& exclude);

#endif //MINIMIZE_HPP
