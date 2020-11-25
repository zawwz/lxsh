#include "minimize.hpp"

#include <map>
#include <regex>
#include <cmath>

#include "recursive.hpp"
#include "parse.hpp"
#include "util.hpp"

std::regex re_var_exclude;
std::regex re_fct_exclude;

const std::regex regex_null;

countmap_t m_vars, m_vardefs, m_varcalls;
countmap_t m_fcts, m_cmds;
std::set<std::string> m_excluded_var, m_excluded_fct, m_excluded_cmd;

bool b_gotvar=false, b_gotfct=false, b_gotcmd=false;

// requires

void require_rescan_var()
{
  b_gotvar = false;
  m_vars.clear();
  m_vardefs.clear();
  m_varcalls.clear();
  m_excluded_var.clear();
}
void require_rescan_fct()
{
  b_gotcmd = false;
  m_fcts.clear();
  m_excluded_fct.clear();
}
void require_rescan_cmd()
{
  b_gotfct = false;
  m_cmds.clear();
  m_excluded_cmd.clear();
}

void require_rescan_all()
{
  require_rescan_var();
  require_rescan_fct();
  require_rescan_cmd();
}

// tools

countmap_t combine_maps(countmap_t const& a, countmap_t const& b)
{
  countmap_t ret;
  for(auto it: a)
  {
    if(!ret.insert( it ).second)
      ret[it.first] += it.second;
  }
  for(auto it: b)
  {
    if(!ret.insert( it ).second)
      ret[it.first] += it.second;
  }
  return ret;
}

void list_map(countmap_t const& map)
{
  uint32_t max=0;
  for(auto it: map)
    if(it.second > max)
      max=it.second;
  for(auto it: map)
    printf("%*d %s\n", (uint32_t)log10(max)+1, it.second, it.first.c_str());
}

std::vector<std::string> get_list(std::string const& in)
{
  return split(in, ", \t\n");
}

std::regex gen_regex_from_list(std::vector<std::string> const& in)
{
  std::string re;
  for(auto it: in)
    re += '('+it+")|";
  if(re.size()>0)
    re.pop_back();
  return std::regex(re);
}

std::vector<std::string> gen_var_excludes(std::string const& in, bool include_reserved)
{
  std::vector<std::string> ret;
  if(include_reserved)
  {
    ret = {RESERVED_VARIABLES, strf("[0-9%s]", SPECIAL_VARS)};
  }
  auto t = get_list(in);
  ret.insert(ret.end(), t.begin(), t.end());
  return ret;
}

std::regex var_exclude_regex(std::string const& in, bool include_reserved)
{
  return gen_regex_from_list(gen_var_excludes(in, include_reserved));
}
std::regex fct_exclude_regex(std::string const& in)
{
  return gen_regex_from_list(get_list(in));
}

bool is_varname(std::string const& in)
{
  if(in.size() <= 0 || !(is_alpha(in[0]) || in[0]== '_') )
    return false;
  uint32_t i=1;
  while(i<in.size() && (is_alphanum(in[i]) || in[i] == '_'))
  {
    i++;
  }
  if(i<in.size() && in[i]!='=')
    return false;
  return true;
}

// object extensions //

std::string get_varname(std::string const& in)
{
  size_t i=in.find('=');
  if(i!=std::string::npos)
    return in.substr(0, i);
  else
    return in;
}

std::string get_varname(arg* in)
{
  if(in->sa.size() < 1 || in->sa[0]->type != _obj::subarg_string)
    return "";
  std::string str = in->sa[0]->generate(0);
  if(in->sa.size() >= 1 && is_varname(str))
    return get_varname(str);
  return "";
}

bool cmd_is_argvar(std::string const& in)
{
  return in == "export" || in == "unset" || in == "local" || in == "read";
}

bool cmd::is_argvar()
{
  return cmd_is_argvar(this->firstarg_string());
}

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

// GET //

bool r_get_var(_obj* in, countmap_t* defmap, countmap_t* callmap)
{
  switch(in->type)
  {
    case _obj::subarg_variable: {
      variable_subarg* t = dynamic_cast<variable_subarg*>(in);
      if(!callmap->insert( std::make_pair(t->varname, 1) ).second)
        (*callmap)[t->varname]++;
    }; break;
    case _obj::subarg_manipulation: {
      manipulation_subarg* t = dynamic_cast<manipulation_subarg*>(in);
      if(!callmap->insert( std::make_pair(t->varname, 1) ).second)
        (*callmap)[t->varname]++;
    }; break;
    case _obj::block_for: {
      for_block* t = dynamic_cast<for_block*>(in);
      if(!defmap->insert( std::make_pair(t->varname, 1) ).second)
        (*defmap)[t->varname]++;
    }; break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      for(auto it: t->var_assigns)
        if(!defmap->insert( std::make_pair(it.first, 1) ).second)
          (*defmap)[it.first]++;
      if(t->is_argvar())
      {
        for(uint32_t i=1; i<t->args->size(); i++)
        {
          std::string varname=get_varname(t->args->args[i]);
          if( varname != "" && !defmap->insert( std::make_pair(varname, 1) ).second )
            (*defmap)[varname]++;
        }
      }
    }; break;
    default: break;
  }
  return true;
}

bool r_get_cmd(_obj* in, countmap_t* all_cmds)
{
  switch(in->type)
  {
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      std::string cmdname = t->firstarg_string();
      if(cmdname != "" && !all_cmds->insert( std::make_pair(cmdname, 1) ).second)
        (*all_cmds)[cmdname]++;
    }; break;
    default: break;
  }
  return true;
}

bool r_get_fct(_obj* in, countmap_t* fct_map)
{
  switch(in->type)
  {
    case _obj::block_function: {
      function* t = dynamic_cast<function*>(in);
      if(!fct_map->insert( std::make_pair(t->name, 1) ).second)
        (*fct_map)[t->name]++;
    }; break;
    default: break;
  }
  return true;
}

// REPLACE //

bool r_replace_var(_obj* in, std::map<std::string,std::string>* varmap)
{
  switch(in->type)
  {
    case _obj::subarg_variable: {
      variable_subarg* t = dynamic_cast<variable_subarg*>(in);
      auto el=varmap->find(t->varname);
      if(el!=varmap->end())
        t->varname = el->second;
    }; break;
    case _obj::subarg_manipulation: {
      manipulation_subarg* t = dynamic_cast<manipulation_subarg*>(in);
      auto el=varmap->find(t->varname);
      if(el!=varmap->end())
        t->varname = el->second;
    }; break;
    case _obj::block_for: {
      for_block* t = dynamic_cast<for_block*>(in);
      auto it=varmap->find(t->varname);
      if(it!=varmap->end())
        t->varname = it->second;
    }; break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      for(auto it=t->var_assigns.begin() ; it!=t->var_assigns.end() ; it++)
      {
        auto el=varmap->find(it->first);
        if(el!=varmap->end())
          it->first = el->second;
      }
      for(auto it: t->subarg_vars())
      {
        string_subarg* t = dynamic_cast<string_subarg*>(it);
        auto el=varmap->find(get_varname(t->val));
        if(el!=varmap->end())
        {
          size_t tpos=t->val.find('=');
          if(tpos == std::string::npos)
            t->val = el->second;
          else
            t->val = el->second + t->val.substr(tpos);
        }
      }
    }; break;
    default: break;
  }
  return true;
}

bool r_replace_fct(_obj* in, std::map<std::string,std::string>* fctmap)
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

// DELETE //

bool r_delete_fct(_obj* in, std::set<std::string>* fcts)
{
  switch(in->type)
  {
    case _obj::_list: {
      list* t = dynamic_cast<list*>(in);
      for(uint32_t i=0; i<t->cls.size(); i++)
      {
        block* tb = t->cls[i]->first_block();
        if(tb != nullptr && tb->type == _obj::block_function)
        {
          function* fc = dynamic_cast<function*>(tb);
          if(fcts->find(fc->name)!=fcts->end())
          {
            delete t->cls[i];
            t->cls.erase(t->cls.begin()+i);
            i--;
          }
        }
      }
    }
    default: break;
  }
  return true;
}

bool r_delete_var(_obj* in, std::set<std::string>* vars)
{
  switch(in->type)
  {
    case _obj::_list: {
      list* t = dynamic_cast<list*>(in);
      for(uint32_t i=0; i<t->cls.size(); i++)
      {
        block* tb = t->cls[i]->first_block();
        bool to_delete=false;
        if(tb != nullptr && tb->type == _obj::block_cmd)
        {
          cmd* c = dynamic_cast<cmd*>(tb);

          for(uint32_t j=0; j<c->var_assigns.size(); j++)
          {
            if( vars->find(c->var_assigns[j].first) != vars->end() )
            {
              delete c->var_assigns[j].second;
              c->var_assigns.erase(c->var_assigns.begin()+j);
              j--;
            }
          }

          if(c->is_argvar())
          {
            for(uint32_t j=1; j<c->args->size(); j++)
            {
              std::string varname=get_varname(c->args->args[j]);
              if( varname != "" && vars->find( varname ) != vars->end() )
              {
                delete c->args->args[j];
                c->args->args.erase(c->args->args.begin()+j);
                j--;
              }
            }
            if(c->args->size()<=1)
              to_delete=true;
          }
          if(c->var_assigns.size()<=0 && c->arglist_size()<=0)
            to_delete=true;

        }
        if(to_delete)
        {
          delete t->cls[i];
          t->cls.erase(t->cls.begin()+i);
          i--;
        }
      }
    }
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

std::map<std::string,std::string> gen_minimal_map(countmap_t const& vars, std::set<std::string> excluded)
{
  std::map<std::string,std::string> ret;
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

// map getters

void varmap_get(_obj* in, std::regex const& exclude)
{
  if(!b_gotvar)
  {
    b_gotvar=true;
    recurse(r_get_var, in, &m_vardefs, &m_varcalls);
    m_vars = combine_maps(m_vardefs, m_varcalls);
    m_excluded_var = prune_matching(m_vars, exclude);
  }
}

void fctmap_get(_obj* in, std::regex const& exclude)
{
  if(!b_gotfct)
  {
    b_gotfct=true;
    recurse(r_get_fct, in, &m_fcts);
    m_excluded_fct = prune_matching(m_fcts, exclude);
  }
}

void cmdmap_get(_obj* in, std::regex const& exclude)
{
  if(!b_gotcmd)
  {
    b_gotcmd=true;
    recurse(r_get_cmd, in, &m_cmds);
    m_excluded_fct = prune_matching(m_cmds, exclude);
  }
}

// calls

void minimize_var(_obj* in, std::regex const& exclude)
{
  // countmap_t vars;
  std::set<std::string> excluded;
  std::map<std::string,std::string> varmap;
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
  std::set<std::string> allcmds, excluded;
  std::map<std::string,std::string> fctmap;
  // get fcts and cmds
  fctmap_get(in, exclude);
  cmdmap_get(in, regex_null);
  // concatenate cmds and excluded commands
  allcmds=map_to_set(m_cmds);
  concat_sets(allcmds, m_excluded_fct);
  // create mapping
  fctmap=gen_minimal_map(m_fcts, allcmds);
  // perform replace
  recurse(r_replace_fct, in, &fctmap);
  require_rescan_fct();
  require_rescan_cmd();
}

bool delete_unused_fct(_obj* in, std::regex const& exclude)
{
  std::set<std::string> unused;
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
  std::set<std::string> unused;
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
}

void list_vars(_obj* in, std::regex const& exclude)
{
  varmap_get(in, exclude);
  list_map(m_vars);
}

void list_var_defs(_obj* in, std::regex const& exclude)
{
  varmap_get(in, exclude);
  list_map(m_vardefs);
}

void list_var_calls(_obj* in, std::regex const& exclude)
{
  varmap_get(in, exclude);
  list_map(m_varcalls);
}

void list_fcts(_obj* in, std::regex const& exclude)
{
  fctmap_get(in, exclude);
  list_map(m_fcts);
}

void list_cmds(_obj* in, std::regex const& exclude)
{
  cmdmap_get(in, exclude);
  list_map(m_cmds);
}
