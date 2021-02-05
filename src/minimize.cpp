#include "minimize.hpp"

#include "recursive.hpp"
#include "processing.hpp"
#include "util.hpp"

std::vector<subarg*> cmd::subarg_vars()
{
  std::vector<subarg*> ret;
  if(args==nullptr || args->size()<=0)
    return ret;

  if(this->is_argvar())
  {
    for(uint32_t i=1; i<args->size(); i++)
    {
      arg* ta = args->args[i];
      if(ta->sa.size() < 1 || ta->sa[0]->type != _obj::subarg_string)
        continue;
      if(ta->sa.size() >= 1 && is_varname(ta->sa[0]->generate(0)))
        ret.push_back(ta->sa[0]);
    }
  }

  return ret;
}

/** RECURSIVES **/

bool r_replace_fct(_obj* in, strmap_t* fctmap)
{
  switch(in->type)
  {
    case _obj::block_function: {
      function* t = dynamic_cast<function*>(in);
      auto el=fctmap->find(t->name);
      if(el!=fctmap->end())
        t->name = el->second;
    }; break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      std::string cmdname = t->firstarg_string();
      auto el=fctmap->find(cmdname);
      if(el!=fctmap->end())
      {
        delete t->args->args[0];
        t->args->args[0] = new arg(el->second);
      }
    }; break;
    default: break;
  }
  return true;
}

bool r_replace_var(_obj* in, strmap_t* varmap)
{
  switch(in->type)
  {
    case _obj::_variable: {
      variable* t = dynamic_cast<variable*>(in);
      auto el=varmap->find(t->varname);
      if(el!=varmap->end())
        t->varname = el->second;
    }; break;
    default: break;
  }
  return true;
}

/** NAME MINIMIZING **/

char nchar(uint32_t n)
{
  if(n<26)
    return 'a'+n;
  else if(n<52)
    return 'A'+(n-26);
  else if(n==52)
    return '_';
  else if(n<63)
    return '0'+(n-53);
  else
    return 0;
}

std::string minimal_name(uint32_t n)
{
  if(n<53)
  {
    std::string ret;
    ret += nchar(n);
    return ret;
  }
  else
  {
    uint32_t k=n%53;
    uint32_t q=n/53;
    std::string ret;
    ret += nchar(k);
    ret += nchar(q);
    while(q>64)
    {
      q /= 64;
      ret += nchar(q);
    }
    return ret;
  }
}

// vars: input variables
// excluded: excluded variables to make sure there is no collision
strmap_t gen_minimal_map(countmap_t const& vars, set_t const& excluded)
{
  strmap_t ret;
  auto ordered = sort_by_value(vars);
  uint32_t n=0;
  for(std::pair<std::string,uint32_t> it: ordered)
  {
    std::string newname;
    do {
      newname = minimal_name(n);
      n++;
    } while( excluded.find(newname) != excluded.end() );
    ret.insert(std::make_pair(it.first, newname));
  }
  return ret;
}

// calls

void minimize_var(_obj* in, std::regex const& exclude)
{
  // countmap_t vars;
  set_t excluded;
  strmap_t varmap;
  // get vars
  varmap_get(in, exclude);
  // create mapping
  varmap=gen_minimal_map(m_vars, m_excluded_var);
  // perform replace
  recurse(r_replace_var, in, &varmap);
  require_rescan_var();
}

void minimize_fct(_obj* in, std::regex const& exclude)
{
  // countmap_t fcts, cmdmap;
  set_t allcmds, excluded, unsets;
  strmap_t fctmap;
  // get fcts and cmds
  fctmap_get(in, exclude);
  cmdmap_get(in, regex_null);
  recurse(r_get_unsets, in, &unsets);
  // concatenate cmds and excluded commands
  allcmds=map_to_set(m_cmds);
  concat_sets(allcmds, m_excluded_fct);
  concat_sets(allcmds, unsets);
  // create mapping
  fctmap=gen_minimal_map(m_fcts, allcmds);
  // perform replace
  recurse(r_replace_fct, in, &fctmap);
  require_rescan_fct();
  require_rescan_cmd();
}

bool delete_unused_fct(_obj* in, std::regex const& exclude)
{
  set_t unused;
  // get fcts and cmds
  fctmap_get(in, exclude);
  cmdmap_get(in, regex_null);
  // find unused fcts
  for(auto it: m_fcts)
  {
    if(m_cmds.find(it.first) == m_cmds.end())
      unused.insert(it.first);
  }
  // perform deletion
  if(unused.size()>0)
  {
    recurse(r_delete_fct, in, &unused);
    require_rescan_all();
    return true;
  }
  else
    return false;
}

bool delete_unused_var(_obj* in, std::regex const& exclude)
{
  set_t unused;
  // get fcts and cmds
  varmap_get(in, exclude);
  // find unused vars
  for(auto it: m_vardefs)
  {
    if(m_varcalls.find(it.first) == m_varcalls.end())
      unused.insert(it.first);
  }
  // perform deletion
  if(unused.size()>0)
  {
    recurse(r_delete_var, in, &unused);
    require_rescan_all();
    return true;
  }
  else
    return false;
}

void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude)
{
  while(delete_unused_fct(in, fct_exclude) || delete_unused_var(in, var_exclude));
  // keep deleting until both no function and no variables were deleted
}
