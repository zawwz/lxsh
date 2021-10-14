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

const char* singlequote_escape_char=" \\\t!\"()|&*?~><#";
const char* doublequote_escape_char="  \t'|&\\*()?~><#";
uint32_t count_escape_chars(std::string const& in, bool doublequote)
{
  uint32_t r=0;
  for(uint32_t i=0; i<in.size(); i++)
  {
    if(doublequote && is_in(in[i], doublequote_escape_char))
      r++;
    else if(!doublequote && is_in(in[i], singlequote_escape_char))
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
          if( is_in(val[k], singlequote_escape_char) )
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
        // iterate subargs
        if(t->sa[i]->type == _obj::subarg_string)
        {
          // has to be a string
          string_subarg* ss = dynamic_cast<string_subarg*>(t->sa[i]);
          bool prev_is_var=false;
          if(i>0 && t->sa[i-1]->type == _obj::subarg_variable)
          {
            // previous subarg is a direct variable (removing a quote could change variable name)
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
    case _obj::_redirect: {
      // for redirects: don't minify quotes on here documents
      redirect* t = dynamic_cast<redirect*>(in);
      if(t->here_document != nullptr)
      {
        recurse(r_minify_useless_quotes, t->target);
        for(auto it: t->here_document->sa)
        {
          if(it->type!=_obj::subarg_string) {
            recurse(r_minify_useless_quotes, it);
          }
        }
        // don't recurse on the rest
        return false;
      }
    } break;
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
  // concatenate excluded and reserved
  concat_sets(excluded, m_excluded_var);
  concat_sets(excluded, all_reserved_words);
  // create mapping
  varmap=gen_minimal_map(m_vars, excluded);
  // perform replace
  recurse(r_replace_var, in, &varmap);
  require_rescan_var();
}

void minify_fct(_obj* in, std::regex const& exclude)
{
  // countmap_t fcts, cmdmap;
  set_t excluded, unsets;
  strmap_t fctmap;
  // get fcts and cmds
  fctcmdmap_get(in, exclude, regex_null);
  recurse(r_get_unsets, in, &unsets);
  // concatenate cmds, excluded and reserved
  excluded=map_to_set(m_cmds);
  exclude_sets(excluded, map_to_set(m_fcts));
  concat_sets(excluded, m_excluded_fct);
  concat_sets(excluded, unsets);
  concat_sets(excluded, all_reserved_words);
  // create mapping
  m_fcts = combine_common(m_fcts, m_cmds);
  fctmap=gen_minimal_map(m_fcts, excluded);
  // perform replace
  recurse(r_replace_fct, in, &fctmap);
  require_rescan_fct();
  require_rescan_cmd();
}

bool delete_unused_fct(_obj* in, std::regex const& exclude)
{
  set_t unused;
  // get fcts and cmds
  fctcmdmap_get(in, exclude, regex_null);
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

bool delete_unused_both(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude)
{
  set_t unused_var, unused_fct;
  // get all
  allmaps_get(in, var_exclude, fct_exclude, regex_null);
  // find unused
  for(auto it: m_vardefs)
  {
    if(it.first!="" && m_varcalls.find(it.first) == m_varcalls.end())
      unused_var.insert(it.first);
  }
  for(auto it: m_fcts)
  {
    if(m_cmds.find(it.first) == m_cmds.end())
      unused_fct.insert(it.first);
  }
  if(unused_var.size()>0 || unused_fct.size()>0)
  {
    recurse(r_delete_varfct, in, &unused_var, &unused_fct);
    require_rescan_all();
    return true;
  }
  return false;
}

void delete_unused(_obj* in, std::regex const& var_exclude, std::regex const& fct_exclude)
{
  while(delete_unused_both(in, var_exclude, fct_exclude));
  // keep deleting until both no deletion
}


// minify ${var} to $var
bool r_minify_empty_manip(_obj* in)
{
  switch(in->type)
  {
    case _obj::_arg: {
      arg* t = dynamic_cast<arg*>(in);
      for(uint32_t i=0; i<t->sa.size(); i++)
      {
        if(t->sa[i]->type == _obj::subarg_variable)
        {
          // has to be a variable
          variable_subarg* ss = dynamic_cast<variable_subarg*>(t->sa[i]);
          if(ss->var->is_manip)
          {
            // if is a manip: possibility to skip it
            if(ss->var->index != nullptr) // is a var bash array: skip
              return true;
            if(i+1<t->sa.size() && t->sa[i+1]->type == _obj::subarg_string)
            {
              // if next subarg is a string: check its first char
              string_subarg* ss = dynamic_cast<string_subarg*>(t->sa[i+1]);
              char c = ss->val[0];
              // if its first would extend the var name: skip
              if(is_alphanum(c) || c == '_')
                continue;
            }
            // if has no actual manipulation operation: set it to not manip
            if(ss->var->manip == nullptr || ss->var->manip->sa.size() == 0)
              ss->var->is_manip = false;
          }
        }
      }
    }; break;
    default: break;
  }
  return true;
}

block* do_one_minify_single_block(block* in)
{
  block* ret=nullptr;
  list* l=nullptr;
  if(in->type == _obj::block_brace)
    l = dynamic_cast<brace*>(in)->lst;
  else if(in->type == _obj::block_subshell)
    l = dynamic_cast<subshell*>(in)->lst;

  if(l == nullptr)
    return nullptr;

  // not a single cmd/block: not applicable
  if(l->cls.size() != 1 || l->cls[0]->pls.size() != 1 || l->cls[0]->pls[0]->cmds.size() != 1)
    return nullptr;

  ret = l->cls[0]->pls[0]->cmds[0];

  // if is a subshell and has some env set: don't remove it
  if(in->type == _obj::block_subshell && has_env_set(ret))
    return nullptr;

  return ret;
}

bool r_minify_single_block(_obj* in)
{
  switch(in->type)
  {
    case _obj::_pipeline: {
      bool has_operated=false;
      do
      {
        // loop operating on current
        // (if has operated, current object has changed)
        has_operated=false;
        pipeline* t = dynamic_cast<pipeline*>(in);
        for(uint32_t i=0; i<t->cmds.size(); i++)
        {
          block* ret = do_one_minify_single_block(t->cmds[i]);
          if(ret != nullptr) {
            // concatenate redirects
            for(uint32_t j=0; j<t->cmds[i]->redirs.size(); j++)
            ret->redirs.insert(ret->redirs.begin()+j, t->cmds[i]->redirs[j]);

            // deindex
            t->cmds[i]->redirs.resize(0);
            if(t->cmds[i]->type == _obj::block_brace)
            dynamic_cast<brace*>(t->cmds[i])->lst->cls[0]->pls[0]->cmds[0] = nullptr;
            else if(t->cmds[i]->type == _obj::block_subshell)
            dynamic_cast<subshell*>(t->cmds[i])->lst->cls[0]->pls[0]->cmds[0] = nullptr;

            // replace value
            delete t->cmds[i];
            t->cmds[i] = ret;

            has_operated=true;
          }
        }
      }
      while(has_operated);
    }; break;
    default: break;
  }
  return true;
}

bool r_has_backtick(_obj* in, bool* r)
{
  if(*r)
    return false;
  switch(in->type)
  {
    case _obj::subarg_subshell: {
      subshell_subarg* t = dynamic_cast<subshell_subarg*>(in);
      if(t->backtick) {
        *r = true;
        return false;
      }
    }; break;
    default: break;
  }
  return true;
}

bool r_minify_backtick(_obj* in)
{
  switch(in->type)
  {
    case _obj::subarg_subshell: {
      subshell_subarg* t = dynamic_cast<subshell_subarg*>(in);
      if(!t->backtick) {
        bool has_backtick_child=false;
        recurse(r_has_backtick, t->sbsh, &has_backtick_child);
        if(has_backtick_child)
          return false;
        t->backtick = true;
      }
      return false;
    }; break;
    default: break;
  }
  return true;
}

bool r_minify(_obj* in)
{
  r_minify_empty_manip(in);
  r_minify_single_block(in);
  r_minify_useless_quotes(in);
  r_do_string_processor(in);
  return true;
}

void minify_generic(_obj* in)
{
  recurse(r_minify, in);
  recurse(r_minify_backtick, in);
}
