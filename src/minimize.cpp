#include "minimize.hpp"

#include <map>
#include <regex>
#include <cmath>

#include "recursive.hpp"
#include "parse.hpp"
#include "util.hpp"

std::regex re_var_exclude;
std::regex re_fct_exclude;

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

std::vector<std::string> gen_var_excludes(std::string const& in)
{
  std::vector<std::string> ret = {RESERVED_VARIABLES, strf("[0-9%s]", SPECIAL_VARS)};
  auto t = get_list(in);
  ret.insert(ret.end(), t.begin(), t.end());
  return ret;
}

std::regex var_exclude_regex(std::string const& in)
{
  return gen_regex_from_list(gen_var_excludes(in));
}
std::regex fct_exclude_regex(std::string const& in)
{
  return gen_regex_from_list(get_list(in));
}

std::vector<subarg*> cmd::arg_vars()
{
  std::vector<subarg*> ret;
  if(args==nullptr || args->size()<=0)
    return ret;
  std::string cmdname=this->firstarg_string();

  if(cmdname == "export" || cmdname == "unset" || cmdname == "local" || cmdname == "read")
  {
    for(uint32_t i=1; i<args->size(); i++)
    {
      arg* ta = args->args[i];
      if(ta->sa.size() < 1 || ta->sa[0]->type != _obj::subarg_string)
        continue;
      if(ta->sa.size() == 1)
      {
        if( std::regex_match(ta->sa[0]->generate(0), std::regex("[a-zA-Z_][0-9a-zA-Z_]*([=](.*)?)?") ) )
          ret.push_back(ta->sa[0]);
      }
      else if(ta->sa.size() > 1)
      {
        if( std::regex_match(ta->sa[0]->generate(0), std::regex("[a-zA-Z_][0-9a-zA-Z_]*=.*") ) )
          ret.push_back(ta->sa[0]);
      }
    }
  }

  return ret;
}

std::string get_varname(subarg* in)
{
  if(in->type != _obj::subarg_string)
    return "";
  std::string& t=dynamic_cast<string_subarg*>(in)->val;
  size_t i=t.find('=');
  if(i!=std::string::npos)
    return t.substr(0, i);
  else
    return t;
}

/** VAR RECURSE **/

bool get_map_varname(_obj* in, std::map<std::string,uint32_t>* variable_map)
{
  if(variable_map == nullptr)
    return false;
  switch(in->type)
  {
    case _obj::subarg_variable: {
      variable_subarg* t = dynamic_cast<variable_subarg*>(in);
      if(!variable_map->insert( std::make_pair(t->varname, 1) ).second)
        (*variable_map)[t->varname]++;
    }; break;
    case _obj::subarg_manipulation: {
      manipulation_subarg* t = dynamic_cast<manipulation_subarg*>(in);
      if(!variable_map->insert( std::make_pair(t->varname, 1) ).second)
        (*variable_map)[t->varname]++;
    }; break;
    case _obj::block_for: {
      for_block* t = dynamic_cast<for_block*>(in);
      if(!variable_map->insert( std::make_pair(t->varname, 1) ).second)
        (*variable_map)[t->varname]++;
    }; break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      for(auto it: t->var_assigns)
        if(!variable_map->insert( std::make_pair(it.first, 1) ).second)
          (*variable_map)[it.first]++;
      for(auto it: t->arg_vars())
      {
        std::string varname=get_varname(it);
        if(!variable_map->insert( std::make_pair(varname, 1) ).second)
          (*variable_map)[varname]++;
      }
    }; break;
    default: break;
  }
  return true;
}

bool replace_varname(_obj* in, std::map<std::string,std::string>* varmap)
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
      for(auto it: t->arg_vars())
      {
        string_subarg* t = dynamic_cast<string_subarg*>(it);
        auto el=varmap->find(get_varname(t));
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

/** FCT RECURSE **/

bool get_map_cmd(_obj* in, std::map<std::string,uint32_t>* all_cmds)
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

bool get_map_fctname(_obj* in, std::map<std::string,uint32_t>* fct_map)
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

bool replace_fctname(_obj* in, std::map<std::string,std::string>* fctmap)
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

bool delete_fcts(_obj* in, std::set<std::string>* fcts)
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

/** name things **/

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

std::map<std::string,std::string> gen_map(std::map<std::string,uint32_t> const& vars, std::set<std::string> excluded)
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

void minimize_var(_obj* in, std::regex exclude)
{
  std::map<std::string,uint32_t> vars;
  std::set<std::string> excluded;
  std::map<std::string,std::string> varmap;
  // get vars
  recurse(get_map_varname, in, &vars);
  // remove excluded
  excluded = prune_matching(vars, exclude);
  // create mapping
  varmap=gen_map(vars, excluded);
  // perform replace
  recurse(replace_varname, in, &varmap);
}

void minimize_fct(_obj* in, std::regex exclude)
{
  std::map<std::string,uint32_t> fcts, cmdmap;
  std::set<std::string> allcmds, excluded;
  std::map<std::string,std::string> fctmap;
  // get fcts
  recurse(get_map_fctname, in, &fcts);
  // get cmds
  recurse(get_map_cmd, in, &cmdmap);
  allcmds=map_to_set(cmdmap);
  // remove excluded
  excluded = prune_matching(fcts, exclude);
  // concatenate excluded to commands
  concat_sets(allcmds, excluded);
  // create mapping
  fctmap=gen_map(fcts, allcmds);
  // perform replace
  recurse(replace_fctname, in, &fctmap);
}

void delete_unused_fct(_obj* in, std::regex exclude)
{
  std::map<std::string,uint32_t> fctmap, cmdmap;
  std::set<std::string> unused;
  // get fcts
  recurse(get_map_fctname, in, &fctmap);
  // get cmds
  recurse(get_map_cmd, in, &cmdmap);
  // remove excluded
  prune_matching(fctmap, exclude);
  for(auto it: fctmap)
  {
    if(cmdmap.find(it.first) == cmdmap.end())
      unused.insert(it.first);
  }
  if(unused.size()>0)
    recurse(delete_fcts, in, &unused);
}

void list_stuff(_obj* in, std::regex exclude, bool (&fct)(_obj*,std::map<std::string,uint32_t>*) )
{
  std::map<std::string,uint32_t> map;
  recurse(fct, in, &map);
  prune_matching(map, exclude);
  uint32_t max=0;
  for(auto it: map)
    if(it.second > max)
      max=it.second;
  for(auto it: map)
    printf("%*d %s\n", (uint32_t)log10(max)+1, it.second, it.first.c_str());
}

void list_vars(_obj* in, std::regex exclude)
{
  list_stuff(in, exclude, get_map_varname);
}

void list_fcts(_obj* in, std::regex exclude)
{
  list_stuff(in, exclude, get_map_fctname);
}

void list_cmds(_obj* in, std::regex exclude)
{
  list_stuff(in, exclude, get_map_cmd);
}
