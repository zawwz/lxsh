#ifndef EXEC_HPP
#define EXEC_HPP

#include "options.hpp"


void parse_exec(FILE* fd, const char* in, uint32_t size, std::string const& filename="");
inline void parse_exec(FILE* fd, std::string const& in, std::string const& filename="") { parse_exec(fd, in.c_str(), in.size(), filename); }

int exec_process(std::string const& runtime, std::vector<std::string> const& args, std::string const& filecontents, std::string const& file);

#endif //EXEC_HPP
