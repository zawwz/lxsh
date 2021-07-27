#ifndef STRUC_HELPER_HPP
#define STRUC_HELPER_HPP

#include "struc.hpp"

// makers
arg* make_arg(std::string const& in);

cmd* make_cmd(std::vector<const char*> const& args);
cmd* make_cmd(std::vector<std::string> const& args);
cmd* make_cmd(std::vector<arg*> const& args);
cmd* make_cmd(std::string const& in);

pipeline* make_pipeline(std::vector<block*> const& bls);
pipeline* make_pipeline(std::string const& in);

condlist* make_condlist(std::string const& in);
list* make_list(std::string const& in);

block* make_block(std::string const& in);

// copy
arg* copy(arg* in);
variable* copy(variable* in);

// testers
bool arg_has_char(char c, arg* in);
bool possibly_expands(arg* in);
bool possibly_expands(arglist* in);

// modifiers
void force_quotes(arg* in);
void add_quotes(arg* in);

// operators
inline bool operator==(arg a, std::string const& b) { return a.equals(b); }

#endif //STRUC_HELPER_HPP
