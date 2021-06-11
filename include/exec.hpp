#ifndef EXEC_HPP
#define EXEC_HPP

#include "options.hpp"
#include "parse.hpp"


void parse_exec(FILE* fd, parse_context ct);

int exec_process(std::string const& runtime, std::vector<std::string> const& args, parse_context ct);

#endif //EXEC_HPP
