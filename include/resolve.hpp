#ifndef RESOLVE_HPP
#define RESOLVE_HPP

#include "struc.hpp"

extern std::vector<std::string> included;

std::vector<std::pair<std::string, std::string>> do_include_raw(condlist* cmd, std::string const& filename, std::string* ex_dir=nullptr);
std::pair<std::string, std::string> do_resolve_raw(condlist* cmd, std::string const& filename, std::string* ex_dir=nullptr);

bool add_include(std::string const& file);

void resolve(_obj* sh, std::string* filename);
void resolve(shmain* sh);

std::string _pre_cd(std::string const& filename);
void _cd(std::string const& dir);

#endif //RESOLVE_HPP
