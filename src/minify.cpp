#include "minify.hpp"

#include <fstream>

#include "parse.hpp"
#include "recursive.hpp"
#include "processing.hpp"
#include "util.hpp"

std::vector<subarg_t*> cmd_t::subarg_vars()
{
  std::vector<subarg_t*> ret;
  if(args==nullptr || args->size()<=0)
    return ret;

  if(this->is_argvar())
  {
    for(uint32_t i=1; i<args->size(); i++)
    {
      arg_t* ta = args->args[i];
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
      function_t* t = dynamic_cast<function_t*>(in);
      auto el=fctmap->find(t->name);
      if(el!=fctmap->end())
        t->name = el->second;
    }; break;
    case _obj::block_cmd: {
      cmd_t* t = dynamic_cast<cmd_t*>(in);
      std::string cmdname = t->arg_string(0);
      auto el=fctmap->find(cmdname);
      if(el!=fctmap->end())
      {
        delete t->args->args[0];
        t->args->args[0] = new arg_t(el->second);
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
    case _obj::variable: {
      variable_t* t = dynamic_cast<variable_t*>(in);
      auto el=varmap->find(t->varname);
      if(el!=varmap->end())
        t->varname = el->second;
    }; break;
    default: break;
  }
  return true;
}

const char* singlequote_escape_char=" \\\t!\"()|&*?~><#$";
const char* doublequote_escape_char="  \t'|&\\*()?~><#$";
uint32_t count_escape_char(std::string& in, uint32_t i, bool doublequote, std::string** estr, uint32_t* ei) {
  if( (  doublequote && is_in(in[i], doublequote_escape_char) ) ||
      ( !doublequote && is_in(in[i], singlequote_escape_char) ) ) {
    *estr = &in;
    *ei = i;
    return 1;
  }
  else if(in[i] == '\n') // \n: can't remove quotes
    return 2;
  return 0;
}

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

bool is_varname(const char c) {
  return is_alphanum(c) || c == '_';
}

void do_minify_quotes(arg_t* in)
{
  auto t = in->sa.begin();
  // global loop
  while(true)
  {
    uint32_t i=0;
    // one iteration loop
    while(true)
    {
      bool doublequote=false;
      bool prev_is_var=false;
      bool end_is_var=false;
      bool has_substitution=false;
      std::string* strstart = nullptr;
      uint32_t quotestart=0;
      std::string* strend = nullptr;
      uint32_t quoteend=0;
      std::string* escapestr = nullptr;
      uint32_t escapepos=0;
      uint32_t ce=0;
      // loop to find start of quote
      while(true)
      {
        // reached end: quit
        if(t == in->sa.end())
          return;
        while((*t)->type != _obj::subarg_string)
        {
          // previous is alphanum var: removing quote can change varname
          if((*t)->type == _obj::subarg_variable) {
            subarg_variable_t* vs = dynamic_cast<subarg_variable_t*>(*t);
            if(vs->var != nullptr && !vs->var->is_manip && vs->var->varname.size()>0 && !(is_in(vs->var->varname[0], SPECIAL_VARS) || is_num(vs->var->varname[0]) ) )
              prev_is_var = true;
          }
          else
            prev_is_var = false;
          t++;
          // quit when reached end of arg
          if(t == in->sa.end())
            return;
          i=0;
        }
        std::string& val = dynamic_cast<subarg_string_t*>(*t)->val;
        // don't attempt if <= 2 chars
        if(in->sa.size() == 1 && val.size() <= 2)
          return;
        while(i<val.size() && !( val[i] == '\'' || val[i] == '"') )
        {
          if(val[i] == '\\')
            i++;
          i++;
        }
        // if found: break and go to next step
        if(i<val.size()) {
          if(val[i] == '"')
            doublequote=true;
          strstart=&val;
          quotestart=i;
          i++;
          break;
        }
        else {
          t++;
          i=0;
        }
      } // end of quote start loop
      // loop to end of quote
      while(true)
      {
        // reached end: quit
        if(t == in->sa.end())
          return;
        while((*t)->type != _obj::subarg_string)
        {
          // previous is alphanum var: removing quote can change varname
          if((*t)->type == _obj::subarg_variable) {
            subarg_variable_t* vs = dynamic_cast<subarg_variable_t*>(*t);
            if(vs->var != nullptr && !vs->var->is_manip && vs->var->varname.size()>0 && !(is_in(vs->var->varname[0], SPECIAL_VARS) || is_num(vs->var->varname[0]) ) )
              end_is_var = true;
          }
          else
            end_is_var = false;
          has_substitution=true;
          t++;
          // quit when reached end of arg
          if(t == in->sa.end())
            return;
          i=0;
        }
        std::string& val = dynamic_cast<subarg_string_t*>(*t)->val;
        if(doublequote)
        {
          while(i<val.size() && val[i] != '"')
          {
            if(val[i] == '\\') {
              ce += count_escape_char(val, i++, doublequote, &escapestr, &escapepos);
            }
            ce += count_escape_char(val, i++, doublequote, &escapestr, &escapepos);
          }
          if(i>=val.size()) { // end before finding quote: continue looping
            t++;
            i=0;
            continue;
          }
        }
        else
        {
          while(i<val.size() && val[i] != '\'')
            ce += count_escape_char(val, i++, doublequote, &escapestr, &escapepos);
          if(i>=val.size()) { // end before finding quote: continue looping
            t++;
            i=0;
            continue;
          }
        }
        strend=&val;
        quoteend=i;
        break;
      } // end of quote end loop
      // has a substitution that can expand: don't dequote
      if(!in->forcequoted && has_substitution) {
        i++;
        continue;
      }
      // too many escapes: don't dequote
      if(ce > 1) {
        i++;
        continue;
      }
      // removing quotes changes variable name: don't dequote
      if( ( prev_is_var && quotestart == 0 && strstart->size()>1 && is_varname((*strstart)[1]) ) ||
          ( end_is_var && quoteend == 0 && strend->size()>1 && is_varname((*strend)[1])) ) {
        i++;
        continue;
      }

      // prev char is a $ would create variable names: don't dequote
      if( quotestart >= 1 && (*strstart)[quotestart-1] == '$' && (!doublequote ||
            ( strstart->size()>2 && is_varname((*strstart)[quotestart+1])))
          ) {
        i++;
        continue;
      }

      // do dequote
      strend->erase(quoteend, 1);
      // needs one escape
      if(ce == 1) {
        escapestr->insert(escapepos, "\\");
      }
      strstart->erase(quotestart, 1);

    }
  }

}

void do_minify_dollar(subarg_string_t* in)
{
  std::string& val = in->val;
  for(uint32_t i=0; i<val.size(); i++) {
    // skip singlequote strings
    if(val[i] == '\'') {
      i++;
      while(val[i] != '\'')
        i++;
    }
    // has \$
    if(i+1<val.size() && val[i] == '\\' && val[i+1] == '$') {
      // char after $ is a varname
      if(i+2<val.size() && (is_varname(val[i+2]) || is_in(val[i+2], SPECIAL_VARS)) )
        continue;
      val.erase(i, 1);
    }
  }
}

bool r_minify_useless_quotes(_obj* in)
{
  switch(in->type)
  {
    case _obj::arg: {
      arg_t* t = dynamic_cast<arg_t*>(in);
      do_minify_quotes(t);
    }; break;
    case _obj::subarg_string: {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(in);
      do_minify_dollar(t);
    }; break;
    case _obj::redirect: {
      // for redirects: don't minify quotes on here documents
      redirect_t* t = dynamic_cast<redirect_t*>(in);
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

strmap_t minify_var(_obj* in, std::regex const& exclude)
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
  return varmap;
}

strmap_t minify_fct(_obj* in, std::regex const& exclude)
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
  return fctmap;
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
    case _obj::arg: {
      arg_t* t = dynamic_cast<arg_t*>(in);
      for(uint32_t i=0; i<t->sa.size(); i++)
      {
        if(t->sa[i]->type == _obj::subarg_variable)
        {
          // has to be a variable
          subarg_variable_t* ss = dynamic_cast<subarg_variable_t*>(t->sa[i]);
          if(ss->var->is_manip)
          {
            // if is a manip: possibility to skip it
            if(ss->var->index != nullptr) // is a var bash array: skip
              return true;
            if(i+1<t->sa.size() && t->sa[i+1]->type == _obj::subarg_string)
            {
              // if next subarg is a string: check its first char
              subarg_string_t* ss = dynamic_cast<subarg_string_t*>(t->sa[i+1]);
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

pipeline_t* do_one_minify_single_block(block_t* in)
{
  pipeline_t* ret=nullptr;
  list_t* l=nullptr;
  if(in->type == _obj::block_brace)
    l = dynamic_cast<brace_t*>(in)->lst;
  else if(in->type == _obj::block_subshell)
    l = dynamic_cast<subshell_t*>(in)->lst;

  if(l == nullptr)
    return nullptr;

  // not a single pipeline: not applicable
  if(l->cls.size() != 1 || l->cls[0]->pls.size() != 1)
    return nullptr;

  ret = l->cls[0]->pls[0];

  // if is a subshell and has some env set: don't remove it
  if(in->type == _obj::block_subshell && has_env_set(ret))
    return nullptr;

  // has a non-stdout/stdin redirect: not applicable
  for(auto it: in->redirs) {
    if(!is_in(it->op[0], "<>") )
      return nullptr;
  }

  return ret;
}

bool r_minify_single_block(_obj* in)
{
  switch(in->type)
  {
    case _obj::pipeline: {
      bool has_operated=false;
      do
      {
        // loop operating on current
        // (if has operated, current object has changed)
        has_operated=false;
        pipeline_t* t = dynamic_cast<pipeline_t*>(in);
        for(uint32_t i=0; i<t->cmds.size(); i++)
        {
          pipeline_t* ret = do_one_minify_single_block(t->cmds[i]);
          if(ret != nullptr) {
            // concatenate redirects
            block_t* firstb = ret->cmds[0];
            block_t* lastb = ret->cmds[ret->cmds.size()-1];
            uint32_t j1=0, j2=0;
            for(uint32_t j=0; j<t->cmds[i]->redirs.size(); j++) {
              if(t->cmds[i]->redirs[j]->op[0] == '<') {
                firstb->redirs.insert(firstb->redirs.begin()+j1, t->cmds[i]->redirs[j]);
                j1++;
              }
              else {
                lastb->redirs.insert(lastb->redirs.begin()+j2, t->cmds[i]->redirs[j]);
                j2++;
              }
            }

            // deindex
            t->cmds[i]->redirs.resize(0);
            if(t->cmds[i]->type == _obj::block_brace)
              dynamic_cast<brace_t*>(t->cmds[i])->lst->cls[0]->pls[0] = nullptr;
            else if(t->cmds[i]->type == _obj::block_subshell)
              dynamic_cast<subshell_t*>(t->cmds[i])->lst->cls[0]->pls[0] = nullptr;

            // replace value
            delete t->cmds[i];
            t->cmds.erase(t->cmds.begin()+i);
            for(auto it: ret->cmds) {
              t->cmds.insert(t->cmds.begin()+i, it);
              i++;
            }

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
      subarg_subshell_t* t = dynamic_cast<subarg_subshell_t*>(in);
      if(t->backtick) {
        *r = true;
        return false;
      }
    }; break;
    case _obj::subarg_string: {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(in);
      if(t->val.find('\\') != std::string::npos)
        *r = true;
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
      subarg_subshell_t* t = dynamic_cast<subarg_subshell_t*>(in);
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

// optimisation for processors that don't have recurse-cancellation
bool r_minify(_obj* in)
{
  r_minify_empty_manip(in);
  r_minify_single_block(in);
  r_do_string_processor(in);
  return true;
}

void minify_generic(_obj* in)
{
  recurse(r_minify, in);
  recurse(r_minify_backtick, in);
  recurse(r_minify_useless_quotes, in);
}

std::string gen_minmap(strmap_t const& map, std::string const& prefix)
{
  std::string ret;
  for(auto it: map) {
    ret += strf("%s %s %s\n", prefix.c_str(), it.second.c_str(), it.first.c_str());
  }
  return ret;
}

void read_minmap(std::string const& filepath, strmap_t* varmap, strmap_t* fctmap)
{
  std::ifstream file(filepath);
  std::string ln;
  while(std::getline(file, ln)) {
    size_t s1, s2, s3;
    s1 = ln.find(' ');
    s2 = ln.find(' ', s1+1);
    s3 = ln.find(' ', s2+1);
    std::string type = ln.substr(0, s1);
    std::string from = ln.substr(s1+1, s2-s1-1);
    std::string to   = ln.substr(s2+1, s3-s2-1);
    if(type == "var")
      varmap->insert(std::make_pair(from, to));
    else if(type == "fct")
      fctmap->insert(std::make_pair(from, to));
  }
}
