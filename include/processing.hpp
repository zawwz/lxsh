#ifndef PROCESSING_HPP
#define PROCESSING_HPP

#include <regex>
#include <string>
#include <set>
#include <map>

#include "struc.hpp"

// constants
#define RESERVED_VARIABLES "HOME", "PATH", "SHELL", "PWD", "OPTIND", "OPTARG", "LC_.*", "LANG", "TERM", "RANDOM", "TMPDIR", "IFS"

// types
typedef std::map<std::string,uint32_t> countmap_t;
typedef std::map<std::string,std::string> strmap_t;
typedef std::set<std::string> set_t;

// regexes
extern std::regex re_var_exclude;
extern std::regex re_fct_exclude;
extern const std::regex regex_null;

// Object maps (optimizations)
extern countmap_t m_vars, m_vardefs, m_varcalls;
extern countmap_t m_fcts, m_cmds;
extern set_t m_excluded_var, m_excluded_fct, m_excluded_cmd;


extern bool b_gotvar, b_gotfct, b_gotcmd;

// tools
countmap_t combine_maps(countmap_t const& a, countmap_t const& b);
countmap_t combine_common(countmap_t const& a, countmap_t const& b);

/** map get functions (optimizations) **/

// rescans
void require_rescan_all();
void require_rescan_var();
void require_rescan_fct();
void require_rescan_cmd();

// get objects
void varmap_get(_obj* in, std::regex const& exclude);
void fctmap_get(_obj* in, std::regex const& exclude);
void cmdmap_get(_obj* in, std::regex const& exclude);
void fctcmdmap_get(_obj* in, std::regex const& exclude_fct, std::regex const& exclude_cmd);
void allmaps_get(_obj* in, std::regex const& exclude_var, std::regex const& exclude_fct, std::regex const& exclude_cmd);

/** util functions **/
#ifdef DEBUG_MODE
std::string gen_json_struc(_obj* in);
#endif

// gen regexes
std::regex var_exclude_regex(std::string const& in, bool include_reserved);
std::regex fct_exclude_regex(std::string const& in);

// varnames
bool is_varname(std::string const& in);
std::string get_varname(std::string const& in);
std::string get_varname(arg_t* in);

// list objects
void list_map(countmap_t const& map);
void list_vars(_obj* in, std::regex const& exclude);
void list_var_defs(_obj* in, std::regex const& exclude);
void list_var_calls(_obj* in, std::regex const& exclude);
void list_fcts(_obj* in, std::regex const& exclude);
void list_cmds(_obj* in, std::regex const& exclude);

// recursives
bool r_has_env_set(_obj* in, bool* result);
bool r_get_unsets(_obj* in, set_t* unsets);
bool r_get_var(_obj* in, countmap_t* defmap, countmap_t* callmap);
bool r_get_cmd(_obj* in, countmap_t* all_cmds);
bool r_get_fct(_obj* in, countmap_t* fct_map);
bool r_get_fctcmd(_obj* in, countmap_t* all_cmds, countmap_t* fct_map);
bool r_get_all(_obj* in, countmap_t* defmap, countmap_t* callmap, countmap_t* all_cmds, countmap_t* fct_map);
bool r_delete_fct(_obj* in, set_t* fcts);
bool r_delete_var(_obj* in, set_t* vars);
bool r_delete_varfct(_obj* in, set_t* vars, set_t* fcts);
bool r_do_string_processor(_obj* in);

/** Processing **/

std::set<std::string> find_lxsh_commands(shmain* sh);
void add_unset_variables(shmain* sh, std::regex const& exclude);
bool has_env_set(_obj* in);

void string_processors(_obj* in);

#endif //PROCESSING_HPP
