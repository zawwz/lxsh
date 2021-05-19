#include "processing.hpp"

#include <cmath>

#include "recursive.hpp"
#include "parse.hpp"
#include "util.hpp"
#include "shellcode.hpp"
#include "struc_helper.hpp"
#include "options.hpp"
#include "minify.hpp"

#include "errcodes.h"

// Global regex

std::regex re_var_exclude;
std::regex re_fct_exclude;

const std::regex regex_null;

// Object maps

countmap_t m_vars, m_vardefs, m_varcalls;
countmap_t m_fcts, m_cmds;
set_t m_excluded_var, m_excluded_fct, m_excluded_cmd;

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

/** TOOLS **/

// type tools
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

inline std::vector<std::string> get_list(std::string const& in)
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

// Variable checks and extensions

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
  return is_in_vector(in, posix_cmdvar) || is_in_vector(in, bash_cmdvar);
}

bool cmd::is_argvar()
{
  return is_cmdvar;
}

bool cmd::is(std::string const& in)
{
  return in == this->arg_string(0);
}

/** GETTERS **/

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

/** OUTPUT **/

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

/** FUNCTIONS **/

void add_unset_variables(shmain* sh, std::regex const& exclude)
{
  varmap_get(sh, exclude);
  if(m_vars.size()>0)
  {
    cmd* unset_cmd = new cmd;
    unset_cmd->add(new arg("unset"));
    unset_cmd->is_cmdvar=true;
    for(auto it: m_vars)
    {
      unset_cmd->var_assigns.push_back(std::make_pair(new variable(it.first), nullptr));
    }
    condlist* cl = new condlist(unset_cmd);
    sh->lst->cls.insert(sh->lst->cls.begin(), cl);
  }
}

/** RECURSIVES **/

// GET //

bool r_get_var(_obj* in, countmap_t* defmap, countmap_t* callmap)
{
  switch(in->type)
  {
    case _obj::_variable: {
      variable* t = dynamic_cast<variable*>(in);
      if(t->definition)
      {
        if(!defmap->insert( std::make_pair(t->varname, 1) ).second)
          (*defmap)[t->varname]++;
      }
      else
      {
        if(!callmap->insert( std::make_pair(t->varname, 1) ).second)
          (*callmap)[t->varname]++;
      }
    }; break;
    default: break;
  }
  return true;
}

bool r_get_unsets(_obj* in, set_t* unsets)
{
  switch(in->type)
  {
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(in);
      if(t->is("unset"))
      {
        for(auto it: t->var_assigns)
        {
          if(it.first != nullptr)
            unsets->insert(it.first->varname);
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
      std::string cmdname = t->arg_string(0);
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

// DELETE //

bool r_delete_fct(_obj* in, set_t* fcts)
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

bool r_delete_var(_obj* in, set_t* vars)
{
  switch(in->type)
  {
    case _obj::_list: {
      list* t = dynamic_cast<list*>(in);
      for(uint32_t i=0; i<t->cls.size(); i++)
      {
        block* tb = t->cls[i]->first_block();
        bool to_delete=false;
        bool has_deleted=false;
        if(tb != nullptr && tb->type == _obj::block_cmd)
        {
          cmd* c = dynamic_cast<cmd*>(tb);

          for(uint32_t j=0; j<c->var_assigns.size(); j++)
          {
            if( c->var_assigns[j].first != nullptr && vars->find(c->var_assigns[j].first->varname) != vars->end() )
            {
              if(c->var_assigns[j].first != nullptr)
                delete c->var_assigns[j].first;
              if(c->var_assigns[j].second != nullptr)
                delete c->var_assigns[j].second;
              c->var_assigns.erase(c->var_assigns.begin()+j);
              has_deleted=true;
              j--;
            }
          }
          if(has_deleted && c->var_assigns.size()<=0 && (c->arglist_size()<=0 || c->is_cmdvar) )
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

std::set<std::string> find_lxsh_commands(shmain* sh)
{
  std::set<std::string> ret;
  cmdmap_get(sh, regex_null);
  for(auto it: lxsh_extend_fcts)
  {
    if(m_cmds.find(it.first) != m_cmds.end())
    {
      ret.insert(it.first);
    }
  }
  return ret;
}

std::set<std::string> get_processors(std::string const& in)
{
  std::set<std::string> ret;
  if(in.size()>2 && in[0] == '\'' && in[in.size()-1] == '\'')
  {
    uint32_t i=1;
    while(true)
    {
      std::string ln = in.substr(i, in.find('\n', i)-i);
      if(ln.size()>1 && ln[0] == '#' && is_alphanum(ln[1]))
      {
        i+=ln.size();
        ret.insert(get_word(make_context(ln.substr(1)), SEPARATORS).first);
      }
      else
        break;
    }
  }
  return ret;
}

bool r_do_string_processor(_obj* in)
{
  if(in->type == _obj::subarg_string)
  {
    string_subarg* t = dynamic_cast<string_subarg*>(in);
    auto v = get_processors(t->val);
    if(v.find("LXSH_PARSE_MINIFY") != v.end())
    {
      try
      {
        std::string stringcode = t->val.substr(1, t->val.size()-2);
        shmain* tsh = parse_text( stringcode ).first;
        require_rescan_all();
        if(options["remove-unused"])
          delete_unused( tsh, re_var_exclude, re_fct_exclude );
        if(options["minify-quotes"])
          minify_quotes(tsh);
        if(options["minify-var"])
          minify_var( tsh, re_var_exclude );
        if(options["minify-fct"])
          minify_fct( tsh, re_fct_exclude );
        require_rescan_all();
        t->val='\'' + tsh->generate(false, 0) + '\'';
      }
      catch(format_error& e) // if fail: skip processing
      {
        std::cerr << "Exception caused in string processing LXSH_PARSE_MINIFY\n";
        printFormatError(e);
        exit(ERR_RUNTIME);
      }
    }
  }
  return true;
}

void string_processors(_obj* in)
{
  recurse(r_do_string_processor, in);
}

/** JSON **/

std::string quote_string(std::string const& in)
{
  return '"' + stringReplace(in, "\"", "\\\"") + '"';
}

std::string gen_json(std::vector<std::pair<std::string,std::string>> const& vec)
{
  std::string ret;
  for(auto it: vec)
  {
    ret += it.first + ":" + it.second + ',';
  }
  if(ret != "")
    ret.pop_back();
  return "{" + ret + "}";
}

std::string gen_json(std::vector<std::string> const& vec)
{
  std::string ret;
  for(auto it: vec)
  {
    ret += it + ',';
  }
  if(ret != "")
    ret.pop_back();
  return "[" + ret + "]";
}

std::string boolstring(bool in)
{
  if(in)
    return "true";
  else
    return "false";
}

std::string gen_json_struc(_obj* o)
{
  if(o==nullptr)
    return "{}";
  std::vector<std::pair<std::string,std::string>> vec;
  switch(o->type)
  {
    case _obj::_variable :
    {
      variable* t = dynamic_cast<variable*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("variable") ) );
      vec.push_back(std::make_pair(quote_string("varname"), quote_string(t->varname)));
      vec.push_back(std::make_pair(quote_string("definition"), boolstring(t->definition)));
      vec.push_back(std::make_pair(quote_string("index"), gen_json_struc(t->index)));
      vec.push_back(std::make_pair(quote_string("is_manip"), boolstring(t->is_manip) ) );
      vec.push_back(std::make_pair(quote_string("precedence"), boolstring(t->precedence) ) );
      vec.push_back(std::make_pair(quote_string("manip"), gen_json_struc(t->manip) ) );
      break;
    }
    case _obj::_redirect :
    {
      redirect* t = dynamic_cast<redirect*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("redirect") ) );
      vec.push_back(std::make_pair(quote_string("op"), quote_string(t->op)));
      vec.push_back(std::make_pair(quote_string("target"), gen_json_struc(t->target)));
      break;
    }
    case _obj::_arg :
    {
      arg* t = dynamic_cast<arg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arg") ) );
      std::vector<std::string> tvec;
      for(auto it: t->sa)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("sa"), gen_json(tvec)));
      break;
    }
    case _obj::_arglist :
    {
      arglist* t = dynamic_cast<arglist*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arglist") ) );
      std::vector<std::string> tvec;
      for(auto it: t->args)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("args"), gen_json(tvec)));
      break;
    }
    case _obj::_pipeline :
    {
      pipeline* t = dynamic_cast<pipeline*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("pipeline") ) );
      vec.push_back(std::make_pair(quote_string("negated"), boolstring(t->negated) ) );
      std::vector<std::string> tvec;
      for(auto it: t->cmds)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("cmds"), gen_json(tvec)));
      break;
    }
    case _obj::_condlist :
    {
      condlist* t = dynamic_cast<condlist*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("condlist") ) );
      vec.push_back(std::make_pair(quote_string("parallel"), boolstring(t->parallel) ) );
      std::vector<std::string> tvec;
      for(auto it: t->pls)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("pls"), gen_json(tvec)));

      std::vector<std::string> ttvec;
      for(auto it: t->or_ops)
      {
        ttvec.push_back(boolstring(it));
      }
      vec.push_back(std::make_pair(quote_string("or_ops"), gen_json(ttvec)));
      break;
    }
    case _obj::_list :
    {
      list* t = dynamic_cast<list*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("list") ) );
      std::vector<std::string> tvec;
      for(auto it: t->cls)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("cls"), gen_json(tvec)));
      break;
    }
    case _obj::block_subshell :
    {
      subshell* t = dynamic_cast<subshell*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subshell") ) );

      vec.push_back(std::make_pair(quote_string("lst"), gen_json_struc(t->lst)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_brace :
    {
      brace* t = dynamic_cast<brace*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("brace") ) );

      vec.push_back(std::make_pair(quote_string("lst"), gen_json_struc(t->lst)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_main :
    {
      shmain* t = dynamic_cast<shmain*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("main") ) );
      vec.push_back(std::make_pair(quote_string("shebang"), quote_string(t->shebang) ) );

      vec.push_back(std::make_pair(quote_string("lst"), gen_json_struc(t->lst)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_function :
    {
      function* t = dynamic_cast<function*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("function") ) );
      vec.push_back(std::make_pair(quote_string("name"), quote_string(t->name) ) );

      vec.push_back(std::make_pair(quote_string("lst"), gen_json_struc(t->lst)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_cmd :
    {
      cmd* t = dynamic_cast<cmd*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("cmd") ) );

      vec.push_back(std::make_pair(quote_string("args"), gen_json_struc(t->args)));
      vec.push_back(std::make_pair(quote_string("is_cmdvar"), boolstring(t->is_cmdvar)));

      std::vector<std::string> aa;
      for(auto it: t->var_assigns)
      {
        std::vector<std::pair<std::string,std::string>> ttvec;
        ttvec.push_back( std::make_pair(quote_string("var"), gen_json_struc(it.first)) );
        ttvec.push_back( std::make_pair(quote_string("value"), gen_json_struc(it.second)) );
        aa.push_back(gen_json(ttvec));
      }
      vec.push_back(std::make_pair( quote_string("var_assigns"), gen_json(aa)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_case :
    {
      case_block* t = dynamic_cast<case_block*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("case") ) );

      vec.push_back(std::make_pair(quote_string("carg"), gen_json_struc(t->carg)));

      // [ {matchers:[], exec:{}} ]
      // cases
      std::vector<std::string> tt;
      for(auto sc: t->cases)
      {
        std::vector<std::pair<std::string,std::string>> onecase;
        std::vector<std::string> matchers;
        for(auto it: sc.first)
          matchers.push_back(gen_json_struc(it));
        onecase.push_back( std::make_pair(quote_string("matcher"), gen_json(matchers)) );
        onecase.push_back( std::make_pair(quote_string("execution"), gen_json_struc(sc.second)) );
        tt.push_back(gen_json(onecase));
      }
      vec.push_back( std::make_pair(quote_string("cases"),gen_json(tt)) );

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_if :
    {
      if_block* t = dynamic_cast<if_block*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("if") ) );

      std::vector<std::string> condblocks;
      // ifs
      for(auto sc: t->blocks)
      {
        std::vector<std::pair<std::string, std::string> > one_cond;
        one_cond.push_back(std::make_pair(quote_string("condition"), gen_json_struc(sc.first) ) );
        one_cond.push_back(std::make_pair(quote_string("execution"), gen_json_struc(sc.second) ) );
        condblocks.push_back(gen_json(one_cond));
      }
      vec.push_back( std::make_pair(quote_string("blocks"), gen_json(condblocks)) );
      // else
      vec.push_back(std::make_pair(quote_string("else_lst"), gen_json_struc(t->else_lst)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_for :
    {
      for_block* t = dynamic_cast<for_block*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("for") ) );
      vec.push_back(std::make_pair(quote_string("var"), gen_json_struc(t->var)));
      vec.push_back(std::make_pair(quote_string("iter"), gen_json_struc(t->iter)));
      vec.push_back(std::make_pair(quote_string("ops"), gen_json_struc(t->ops)));

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::block_while :
    {
      while_block* t = dynamic_cast<while_block*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("while") ) );
      vec.push_back(std::make_pair(quote_string("cond"), gen_json_struc(t->cond) ) );
      vec.push_back(std::make_pair(quote_string("ops"), gen_json_struc(t->ops) ) );

      std::vector<std::string> tvec;
      for(auto it: t->redirs)
        tvec.push_back(gen_json_struc(it));
      vec.push_back(std::make_pair(quote_string("redirs"), gen_json(tvec)));

      break;
    }
    case _obj::subarg_variable :
    {
      variable_subarg* t = dynamic_cast<variable_subarg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subarg_variable") ) );
      vec.push_back(std::make_pair(quote_string("var"), gen_json_struc(t->var) ) );
      break;
    }
    case _obj::subarg_subshell :
    {
      subshell_subarg* t = dynamic_cast<subshell_subarg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subarg_subshell") ) );
      vec.push_back(std::make_pair(quote_string("sbsh"), gen_json_struc(t->sbsh) ) );
      break;
    }
    case _obj::subarg_procsub :
    {
      procsub_subarg* t = dynamic_cast<procsub_subarg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subarg_procsub") ) );
      vec.push_back(std::make_pair(quote_string("is_output"), boolstring(t->is_output) ) );
      vec.push_back(std::make_pair(quote_string("sbsh"), gen_json_struc(t->sbsh) ) );
      break;
    }
    case _obj::subarg_arithmetic :
    {
      arithmetic_subarg* t = dynamic_cast<arithmetic_subarg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subarg_arithmetic") ) );
      vec.push_back(std::make_pair(quote_string("arith"), gen_json_struc(t->arith) ) );
      break;
    }
    case _obj::subarg_string :
    {
      string_subarg* t = dynamic_cast<string_subarg*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("subarg_string") ) );
      vec.push_back(std::make_pair(quote_string("val"), quote_string(t->val) ) );
      break;
    }
    case _obj::arithmetic_variable :
    {
      variable_arithmetic* t = dynamic_cast<variable_arithmetic*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arithmetic_variable") ) );
      vec.push_back(std::make_pair(quote_string("var"), gen_json_struc(t->var) ) );
      break;
    }
    case _obj::arithmetic_subshell :
    {
      subshell_arithmetic* t = dynamic_cast<subshell_arithmetic*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arithmetic_subshell") ) );
      vec.push_back(std::make_pair(quote_string("sbsh"), gen_json_struc(t->sbsh) ) );
      break;
    }
    case _obj::arithmetic_operation :
    {
      operation_arithmetic* t = dynamic_cast<operation_arithmetic*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arithmetic_operation") ) );
      vec.push_back(std::make_pair(quote_string("val1"), gen_json_struc(t->val1) ) );
      vec.push_back(std::make_pair(quote_string("val2"), gen_json_struc(t->val2) ) );
      break;
    }
    case _obj::arithmetic_parenthesis :
    {
      parenthesis_arithmetic* t = dynamic_cast<parenthesis_arithmetic*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arithmetic_parenthesis") ) );
      vec.push_back(std::make_pair(quote_string("val"), gen_json_struc(t->val) ) );
      break;
    }
    case _obj::arithmetic_number :
    {
      number_arithmetic* t = dynamic_cast<number_arithmetic*>(o);
      vec.push_back(std::make_pair(quote_string("type"), quote_string("arithmetic_number") ) );
      vec.push_back(std::make_pair(quote_string("val"), quote_string(t->val) ) );
      break;
    }
  }
  return gen_json(vec);
}
