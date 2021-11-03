#ifndef STRUC_HELPER_HPP
#define STRUC_HELPER_HPP

#include "struc.hpp"

// makers
arg_t* make_arg(std::string const& in);

cmd_t* make_cmd(std::vector<const char*> const& args);
cmd_t* make_cmd(std::vector<std::string> const& args);
cmd_t* make_cmd(std::vector<arg_t*> const& args);
cmd_t* make_cmd(std::string const& in);

pipeline_t* make_pipeline(std::vector<block_t*> const& bls);
pipeline_t* make_pipeline(std::string const& in);

condlist_t* make_condlist(std::string const& in);
list_t* make_list(std::string const& in);

block_t* make_block(std::string const& in);

// copy
arg_t* copy(arg_t* in);
variable_t* copy(variable_t* in);

// testers
bool arg_has_char(char c, arg_t* in);
bool possibly_expands(arg_t* in);
bool possibly_expands(arglist_t* in);

// modifiers
void force_quotes(arg_t* in);
void add_quotes(arg_t* in);

cmd_t* make_printf(arg_t* in);
inline cmd_t* make_printf_variable(std::string const& name) {
  return make_printf(new arg_t(new subarg_variable_t(new variable_t(name))));
}

arithmetic_t* make_arithmetic(arg_t* a);
arithmetic_t* make_arithmetic(arg_t* arg1, std::string op, arg_t* arg2);

// operators
inline bool operator==(arg_t a, std::string const& b) { return a.equals(b); }

#endif //STRUC_HELPER_HPP
