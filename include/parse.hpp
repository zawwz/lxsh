  #ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>

#include <ztd/filedat.hpp>

#define SPACES          " \t"
#define SEPARATORS      " \t\n"
#define ARG_END         " \t\n;#()&|"
#define COMMAND_SEPARATOR  "\n;"
#define CONTROL_END           "#)"
#define PIPELINE_END       "\n;#()&"
#define ARGLIST_END        "\n;#()&|"
#define SPECIAL_TOKENS     "\n;#()&|"
#define ALL_TOKENS         "\n;#()&|{}"

extern std::string g_origin;

std::string import_file(std::string const& path);

shmain* parse(const char* in, uint32_t size);
inline shmain* parse(std::string const& in) { return parse(in.c_str(), in.size()); }

#endif //PARSE_HPP
