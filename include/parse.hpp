#ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>

#include <ztd/filedat.hpp>

extern std::string g_origin;

std::string import_file(std::string const& path);

shmain* parse(const char* in, uint32_t size);
inline shmain* parse(std::string const& in) { return parse(in.c_str(), in.size()); }

#endif //PARSE_HPP
