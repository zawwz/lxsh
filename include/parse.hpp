  #ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>

#include <ztd/filedat.hpp>

#define SPACES          " \t"
#define SEPARATORS      " \t\n"
#define ARG_END         " \t\n;#()&|<>"
#define VARNAME_END     " \t\n;#()&|=\"'\\{}/-+"
#define BLOCK_TOKEN_END " \t\n;#()&|=\"'\\"
#define COMMAND_SEPARATOR  "\n;"
#define CONTROL_END           "#)"
#define PIPELINE_END       "\n;#()&"
#define ARGLIST_END        "\n;#()&|"
#define SPECIAL_TOKENS     "\n;#()&|"
#define ALL_TOKENS         "\n;#()&|{}"

#define SPECIAL_VARS "!#*@$?"

inline bool is_num(char c) { return (c >= '0' && c <= '9'); }
inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_alphanum(char c) { return is_alpha(c) || is_num(c); }

std::string import_file(std::string const& path);

shmain* parse_text(const char* in, uint32_t size, std::string const& filename="");
inline shmain* parse_text(std::string const& in, std::string const& filename="") { return parse_text(in.c_str(), in.size(), filename); }
inline shmain* parse(std::string const& file) { return parse_text(import_file(file), file); }

#endif //PARSE_HPP
