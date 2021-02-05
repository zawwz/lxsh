#ifndef PROCESSING_HPP
#define PROCESSING_HPP

#include <regex>
#include <string>
#include <set>
#include <map>

#include "struc.hpp"

// constants
#define RESERVED_VARIABLES "HOME", "PATH", "SHELL", "PWD", "OPTIND", "OPTARG"

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

/** util functions **/
std::string gen_json_struc(_obj* in);

// gen regexes
std::regex var_exclude_regex(std::string const& in, bool include_reserved);
std::regex fct_exclude_regex(std::string const& in);

// varnames
bool is_varname(std::string const& in);
std::string get_varname(std::string const& in);
std::string get_varname(arg* in);

// list objects
void list_map(countmap_t const& map);
void list_vars(_obj* in, std::regex const& exclude);
void list_var_defs(_obj* in, std::regex const& exclude);
void list_var_calls(_obj* in, std::regex const& exclude);
void list_fcts(_obj* in, std::regex const& exclude);
void list_cmds(_obj* in, std::regex const& exclude);

// recursives
bool r_get_unsets(_obj* in, set_t* unsets);
bool r_get_var(_obj* in, countmap_t* defmap, countmap_t* callmap);
bool r_get_cmd(_obj* in, countmap_t* all_cmds);
bool r_get_fct(_obj* in, countmap_t* fct_map);
bool r_delete_fct(_obj* in, set_t* fcts);
bool r_delete_var(_obj* in, set_t* vars);

/** Processing **/

void add_unset_variables(shmain* sh, std::regex const& exclude);

#endif //PROCESSING_HPP
