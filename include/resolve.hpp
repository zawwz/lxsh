#ifndef RESOLVE_HPP
#define RESOLVE_HPP

#include "struc.hpp"

extern std::vector<std::string> included;

bool add_include(std::string const& file);

void resolve(_obj* sh, shmain* parent);
void resolve(shmain* sh);

#endif //RESOLVE_HPP
