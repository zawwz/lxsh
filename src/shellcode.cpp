#include "shellcode.hpp"

#include "g_shellcode.h"
#include "processing.hpp"
#include "struc_helper.hpp"

const std::map<const std::string, const lxsh_fct> lxsh_extend_fcts = {
  { "_lxsh_random",         { "[K]", "Generate a random number between 0 and 2^(k*8). Default 2", RANDOM_SH} },
  { "_lxsh_random_string",  { "[N]", "Generate a random alphanumeric string of length N. Default 20", RANDOM_STRING_SH} },
  { "_lxsh_random_tmpfile", { "[N]", "Get a random TMP filepath, with N random chars. Default 20", RANDOM_TMPFILE_SH, {"_lxsh_random_string"} } }
};

const std::map<const std::string, const lxsh_fct> lxsh_array_fcts = {
  { "_lxsh_array_create",   { "<VAL...>", "Create an array out of input arguments", ARRAY_CREATE_SH} },
  { "_lxsh_array_get",      { "<ARRAY> <I>",    "Get value from array", ARRAY_GET_SH} },
  { "_lxsh_array_set",      { "<ARRAY> <I> <VAL>", "Set value of array", ARRAY_SET_SH} },
  { "_lxsh_map_create",     { "<VAL...>", "Create a map (associative array) out of input arguments", MAP_CREATE_SH} },
  { "_lxsh_map_get",        { "<MAP> <KEY>",    "Get value from map", MAP_GET_SH} },
  { "_lxsh_map_set",        { "<MAP> <KEY> <VAL>", "Set value of map", MAP_SET_SH} }
};

std::map<const std::string, const lxsh_fct> create_allfcts()
{
  auto r = lxsh_array_fcts;
  for(auto it: lxsh_extend_fcts)
    r.insert(it);
  return r;
}

const std::map<const std::string, const lxsh_fct> lxsh_allfcts = create_allfcts();

void add_lxsh_fcts(shmain* sh, std::set<std::string> fcts)
{
  // resolve dependencies
  for(auto fctname: fcts)
  {
    auto ti=lxsh_allfcts.find(fctname);
    if(ti != lxsh_allfcts.end())
      for(auto dep: ti->second.depends_on)
        fcts.insert(dep);
  }
  // insert functions
  for(auto it: fcts)
  {
    sh->lst->insert(0, make_condlist(lxsh_allfcts.find(it)->second.code) );
  }
  require_rescan_all();
}
