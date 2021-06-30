#include "minify.hpp"


#include "parse.hpp"
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
      std::string cmdname = t->arg_string(0);
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

const char* escaped_char=" \\\t!\"()|&*?~><#";
const char* doublequote_escape_char="  \t'|&\\*()?~><#";
uint32_t count_escape_chars(std::string const& in, bool doublequote)
{
  uint32_t r=0;
  for(uint32_t i=0; i<in.size(); i++)
  {
    if(doublequote && is_in(in[i], doublequote_escape_char))
      r++;
    else if(!doublequote && is_in(in[i], escaped_char))
      r++;
    else if(in[i] == '\n') // \n: can't remove quotes
      return 2;
    else if(in[i] == '$')
    {
      if(i+1>=in.size())
        continue;
      else if(is_in(in[i+1], SPECIAL_VARS) || is_alphanum(in[i+1]) || in[i+1] == '_' || in[i+1] == '(')
      {
        if(doublequote) // doublequote: can't remove otherwise not quoted var
          return 2;
        r++;
      }
    }
  }
  return r;
}

bool is_this_quote(char c, bool is_doublequote)
{
  if(is_doublequote)
    return c == '"';
  else
    return c == '\'';
}

void do_one_minify_quotes(string_subarg* in, bool prev_is_var, bool start_quoted)
{
  std::string& val = in->val;
  if(val.size() <= 1)
    return;
  if(start_quoted) // don't handle start quoted for now
    return;
  if(val[0] == '"' && prev_is_var && (is_alphanum(val[1]) || val[1] == '_') ) // removing quote would change varname: skip
    return;
  if(val[0] == '\'' && prev_is_var && (is_alphanum(val[1]) || val[1] == '_') ) // removing quote would change varname: skip
    return;

  uint32_t i=0, j=0;
  while( i < val.size() )
  {
    bool doublequote=false;
    while(i<val.size() && !( val[i] == '\'' || val[i] == '"') )
    {
      if(val[i] == '\\')
        i++;
      i++;
    }
    if(i>=val.size()) // end before finding quote: exit
      return;
    if(val[i] == '"')
      doublequote=true;

    j=i;
    i++;

    if(doublequote)
    {
      while(i<val.size() && val[i] != '"')
      {
        if(val[i] == '\\')
          i++;
        i++;
      }
      if(i>=val.size()) // end before finding quote: exit
        return;
    }
    else
    {
      while(i<val.size() && val[i] != '\'')
        i++;
      if(i>=val.size()) // end before finding quote: exit
        return;

    }
    uint32_t ce = count_escape_chars(val.substr(j+1, i-j-1), doublequote);
    if(ce == 0)
    {
      val.erase(val.begin()+i);
      val.erase(val.begin()+j);
    }
    else if(ce == 1) // only one char to escape: can save some space
    {
      val.erase(val.begin()+i);
      val.erase(val.begin()+j);
      uint32_t k;
      if(doublequote)
      {
        for(k=j; k<i-1; k++)
        {
          if( is_in(val[k], doublequote_escape_char) )
          break;
        }
      }
      else
      {
        for(k=j; k<i-1; k++)
        {
          if( is_in(val[k], escaped_char) )
          break;
          if( k+1<val.size() && val[k] == '$' && ( is_in(val[k+1], SPECIAL_VARS) || is_alpha(val[k+1]) || val[k+1] == '_' ) )
          break;
        }
      }
      if(k<i-1)
        val.insert(val.begin()+k, '\\');
    }

  }

}

bool r_minify_useless_quotes(_obj* in)
{
  switch(in->type)
  {
    case _obj::_arg: {
      arg* t = dynamic_cast<arg*>(in);
      for(uint32_t i=0; i<t->sa.size(); i++)
      {
        if(t->sa[i]->type == _obj::subarg_string)
        {
          string_subarg* ss = dynamic_cast<string_subarg*>(t->sa[i]);
          bool prev_is_var=false;
          if(i>0 && t->sa[i-1]->type == _obj::subarg_variable)
          {
            variable_subarg* vs = dynamic_cast<variable_subarg*>(t->sa[i-1]);
            if(vs->var != nullptr && vs->var->is_manip == false && vs->var->varname.size()>0 && !(is_in(vs->var->varname[0], SPECIAL_VARS) || is_alpha(vs->var->varname[0]) ) )
              prev_is_var=true;
          }
          if(t->sa.size()==1 && (ss->val=="\"\"" || ss->val=="''") ) // single argument as "" or '': don't minify
            continue;
          do_one_minify_quotes(ss, prev_is_var, i>0 && t->sa[i-1]->quoted);
        }
        //if()
      }
    }; break;
    default: break;
  }
  return true;
}

/** NAME MINIFYING **/

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

void minify_var(_obj* in, std::regex const& exclude)
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

void minify_fct(_obj* in, std::regex const& exclude)
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
  concat_sets(allcmds, all_reserved_words);
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
    if(it.first!="" && m_varcalls.find(it.first) == m_varcalls.end())
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

void minify_quotes(_obj* in)
{
  recurse(r_minify_useless_quotes, in);
}

void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude)
{
  while(delete_unused_fct(in, fct_exclude) || delete_unused_var(in, var_exclude));
  // keep deleting until both no function and no variables were deleted
}
