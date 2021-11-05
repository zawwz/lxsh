#include "debashify.hpp"

#include <algorithm>

#include <ztd/options.hpp>

#include "processing.hpp"
#include "recursive.hpp"
#include "util.hpp"
#include "parse.hpp"
#include "struc_helper.hpp"

#include "g_shellcode.h"


/*
[[ ]] debashifying:
[[ EXPRESSION && EXPRESSION ]] separated into two parts
EXPRESSION : gen_bashtest_cmd
&& , || : debashify_bashtest
*/

// [[ $a = b ]] : quote vars
// [[ a == b ]] : replace == with =
// [[ a = b* ]] : case a in b*) true;; *) false;; esac
// [[ a =~ b ]] : expr a : "b" >/dev/null
block_t* gen_bashtest_cmd(std::vector<arg_t*> args)
{
  block_t* ret = nullptr;

  std::string arg1replace;
  if(args.size() == 3)
  {
    if(args[1]->string() == "==")
      arg1replace="=";
    else if(args[1]->string() == "<")
      arg1replace="-lt";
    else if(args[1]->string() == "<=")
      arg1replace="-le";
    else if(args[1]->string() == ">")
      arg1replace="-gt";
    else if(args[1]->string() == ">=")
      arg1replace="-ge";
  }
  if(arg1replace != "")
  {
    delete args[1];
    args[1] = new arg_t(arg1replace);
  }

  if(args.size() == 3 && args[1]->string() == "=" && (arg_has_char('*', args[2]) || arg_has_char('?', args[2])) )
  {
    // glob matcher: do a case
    delete args[1];
    args[1]=nullptr;
    case_t* tc = new case_t(args[0]);
    tc->cases.push_back( std::make_pair(std::vector<arg_t*>({args[2]}), make_list("true")) );
    tc->cases.push_back( std::make_pair(std::vector<arg_t*>({new arg_t("*")}), make_list("false")) );
    ret = tc;
  }
  else if(args.size() == 3 && args[1]->string() == "=~")
  {
    // regex matcher: use expr
    delete args[1];
    args[1]=nullptr;
    args[2]->insert(0, ".*");
    add_quotes(args[2]);
    ret = make_cmd( std::vector<arg_t*>({ new arg_t("expr"), args[0], new arg_t(":"), args[2] }) );
    ret->redirs.push_back(new redirect_t(">", new arg_t("/dev/null") ));
  }
  else
  {
    // regular [ ]
    cmd_t* t = make_cmd(args);
    t->args->insert(0, new arg_t("["));
    t->add(new arg_t("]"));
    ret = t;
  }
  // arg oblivious replacements:
  // quote variables
  for(auto it: args)
  {
    if(it!=nullptr)
      force_quotes(it);
  }
  return ret;
}

// [[ a && b ]] : [ a ] && [ b ]
bool debashify_bashtest(pipeline_t* pl)
{
  if(pl->cmds.size()<=0)
    return false;

  if(pl->cmds[0]->type != _obj::block_cmd)
    return false;
  cmd_t* in = dynamic_cast<cmd_t*>(pl->cmds[0]);

  if(in->arg_string(0) == "[[")
  {
    brace_t* br = new brace_t(new list_t);
    condlist_t* cl = new condlist_t;
    br->lst->add(cl);

    arg_t* a=nullptr;
    uint32_t j=1;
    bool or_op=false;
    std::string tmpstr;
    for(uint32_t i=1 ; i<in->args->size() ; i++)
    {
      a = in->args->args[i];
      tmpstr = a->string();
      bool logic_op = ( tmpstr == "&&" || tmpstr == "||" );

      if(i >= in->args->size()-1 || logic_op)
      {
        block_t* tbl = gen_bashtest_cmd(std::vector<arg_t*>(in->args->args.begin()+j, in->args->args.begin()+i));
        cl->add(new pipeline_t(tbl), or_op);
        or_op = tmpstr == "||";
        j=i+1;
        if(logic_op)
          delete a;
      }
    }

    delete in->args->args[0];
    delete in->args->args[in->args->args.size()-1];
    in->args->args.resize(0);
    delete in;
    pl->cmds[0] = br;

    return true;
  }

  return false;
}

void warn(std::string const& in)
{
  std::cerr << "WARN: " << in << std::endl;
}

std::string get_declare_opt(cmd_t* in)
{
  if(in->cmd_var_assigns[0].second!=nullptr)
  {
    return in->cmd_var_assigns[0].second->string();
  }
  return "";
}

ztd::option_set gen_echo_opts()
{
  ztd::option_set ret( std::vector<ztd::option>({
    ztd::option('e'),
    ztd::option('E'),
    ztd::option('n')
  }) );
  return ret;
}

bool debashify_echo(pipeline_t* pl)
{
  if(pl->cmds[0]->type != _obj::block_cmd)
    return false;
  cmd_t* in = dynamic_cast<cmd_t*>(pl->cmds[0]);

  std::string const& cmdstr=in->arg_string(0);
  if(cmdstr == "echo")
  {
    bool skip=false;
    ztd::option_set opts=gen_echo_opts();
    std::vector<std::string> args=in->args->strargs(1);
    std::vector<std::string> postargs;

    try
    {
      postargs=opts.process(args, {.ignore_numbers=true, .stop_on_argument=true} );
    }
    catch(ztd::option_error& e)
    {
      skip=true;
    }

    bool enable_interpretation=false;
    bool newline=true;
    bool has_escape_sequence=false;
    bool has_processed_options=false;

    if(!skip && postargs.size() != args.size())
    {
      has_processed_options=true;
      // delete the number of args that were processed
      for(uint32_t i=0; i<args.size()-postargs.size(); i++)
      {
        delete in->args->args[1];
        in->args->args.erase(in->args->args.begin()+1);
      }

      if(opts['e'])
        enable_interpretation=true;
      else if(opts['n'])
        newline=false;
    }

    for(auto it=in->args->args.begin()+1; it!=in->args->args.end(); it++)
    {
      if(!(*it)->is_string() || (*it)->string().find('\\') != std::string::npos)
      {
        has_escape_sequence=true;
        break;
      }
    }

    if(newline && !has_escape_sequence)
    {
      // newline and no potential escape: don't replace, keep echo
      return has_processed_options;
    }
    else
    {
      // replace by printf
      if(!in->args->can_expand())
      {
        // no potential expansion: static number of args
        std::string format_str = "'";
        for(uint32_t i=1; i<in->args->args.size(); i++)
        {
          if(enable_interpretation)
            format_str += "%b ";
          else
            format_str += "%s ";
        }
        format_str.pop_back();
        if(newline)
          format_str += "\\n";
        format_str += '\'';

        in->args->insert(1, new arg_t(format_str));
        delete in->args->args[0];
        in->args->args[0] = new arg_t("printf");
      }
      else
      {
        std::string format_str;
        if(enable_interpretation)
          format_str = "%b";
        else
          format_str = "%s";

        list_t* lst=nullptr;

        // more than 1 arg and first arg can't expand: can split into two printf
        // printf '%s' arg1
        // printf ' %s' args...
        if(in->args->args.size()>2 && !in->args->args[1]->can_expand())
        {
          // extract arg 1
          arg_t* arg1 = in->args->args[1];
          in->args->args.erase(in->args->args.begin()+1);
          delete in->args->args[0];
          in->args->args[0] = new arg_t("printf");

          lst = new list_t;
          lst->add(new condlist_t(make_cmd({new arg_t("printf"), new arg_t(format_str), arg1 })));
          in->args->insert(1, new arg_t("\\ "+format_str) );
          lst->add(new condlist_t(in));
        }
        else
        {
          // can't reliable replace: keep echo if newline
          if(newline)
            return has_processed_options;

          in->args->insert(1, new arg_t(format_str+"\\ "));
          delete in->args->args[0];
          in->args->args[0] = new arg_t("printf");
        }

        if(newline)
        {
          if(lst == nullptr)
          {
            lst = new list_t;
            lst->add(new condlist_t(in));
          }
          lst->add(make_condlist("echo"));
        }

        if(lst != nullptr)
        {
          pl->cmds[0] = new brace_t(lst);
        }
      }
    }

    return true;
  }
  return false;
}

bool debashify_readonly(list_t* in)
{
  bool has_found=false;
  for(uint32_t i=0; i<in->cls.size(); i++)
  {
    // not a cmd: go to next
    if(in->cls[i]->pls[0]->cmds[0]->type != _obj::block_cmd)
      continue;

    cmd_t* c1 = dynamic_cast<cmd_t*>(in->cls[i]->pls[0]->cmds[0]);
    std::string const& cmdstr=c1->arg_string(0);
    if(cmdstr == "readonly")
    {
      has_found=true;
      c1->is_cmdvar=false;
      for(uint32_t i=0; i<c1->cmd_var_assigns.size(); i++)
      {
        if(c1->cmd_var_assigns[i].first == nullptr || c1->cmd_var_assigns[i].second == nullptr)
        {
          if(c1->cmd_var_assigns[i].first != nullptr)
            delete c1->cmd_var_assigns[i].first;
          if(c1->cmd_var_assigns[i].second != nullptr)
            delete c1->cmd_var_assigns[i].second;
          c1->cmd_var_assigns.erase(c1->cmd_var_assigns.begin()+i);
          i--;
        }
      }
      if(c1->cmd_var_assigns.size() == 0)
      {
        delete in->cls[i];
        in->cls.erase(in->cls.begin()+i);
        i--;
      }
      else
      {
        delete c1->args;
        c1->args = new arglist_t;
        c1->var_assigns=c1->cmd_var_assigns;
        c1->cmd_var_assigns.resize(0);
      }
    }
  }
  if(in->cls.size()<=0)
    in->add(make_condlist("true"));
  return has_found;
}

bool debashify_declare(list_t* in, debashify_params* params)
{
  bool has_found=false;
  for(uint32_t i=0; i<in->cls.size(); i++)
  {
    // not a cmd: go to next
    if(in->cls[i]->pls[0]->cmds[0]->type != _obj::block_cmd)
      continue;

    cmd_t* c1 = dynamic_cast<cmd_t*>(in->cls[i]->pls[0]->cmds[0]);
    std::string const& cmdstr=c1->arg_string(0);
    if(cmdstr == "declare" || cmdstr == "typeset")
    {
      std::string const& op = get_declare_opt(c1);
      if(op == "-a")
      {
        for(auto it: c1->cmd_var_assigns)
        {
          if(it.first != nullptr)
          params->arrays[it.first->varname] = false;
        }
      }
      else if(op == "-A")
      {
        for(auto it: c1->cmd_var_assigns)
        {
          if(it.first != nullptr)
          params->arrays[it.first->varname] = true;
        }
      }
      has_found=true;
      delete in->cls[i];
      in->cls.erase(in->cls.begin()+i);
      i--;
    }
  }
  return has_found;
}

cmd_t* make_cmd_varindex(std::string const& strcmd, std::string const& varname, arg_t* index)
{
  cmd_t* c = new cmd_t(new arglist_t);
  // cmd
  c->args->add( new arg_t(strcmd) );
  // cmd "$VAR"
  c->args->add( make_arg("\"$"+varname+"\"") );
  // cmd "$VAR" N
  c->args->add( index );
  return c;
}

subshell_t* do_debashify_array_var_get(variable_t* in, debashify_params* params)
{
  if(in->manip != nullptr)
    throw std::runtime_error("Cannot debashify manipulations on ${VAR[]}");

  std::string varname = in->varname;
  arg_t* index = in->index;
  in->index=nullptr;

  if(index->string() == "*")
  {
    delete index;
    index = new arg_t("\\*");
  }

  cmd_t* c;
  if(params->arrays[varname])
  {
    c = make_cmd_varindex("_lxsh_map_get", varname, index);
    params->require_fct("_lxsh_map_get");
  }
  else
  {
    c = make_cmd_varindex("_lxsh_array_get", varname, index);
    params->require_fct("_lxsh_array_get");
  }

  return new subshell_t(c);
}

subshell_t* do_debashify_random(variable_t* in, debashify_params* params)
{
  if(in->manip != nullptr)
    throw std::runtime_error("Cannot debashify manipulations on ${RANDOM}");
  cmd_t* c = make_cmd("_lxsh_random");
  params->require_fct("_lxsh_random");
  return new subshell_t(c);
}

// does multiple debashifies:
// - array
// - RANDOM
arithmetic_subshell_t* do_debashify_arithmetic_t(arithmetic_t* in, debashify_params* params)
{
  arithmetic_subshell_t* ret = nullptr;
  if(in->type == _obj::arithmetic_variable)
  {
    arithmetic_variable_t* t = dynamic_cast<arithmetic_variable_t*>(in);
    if(t->var != nullptr && t->var->varname == "RANDOM")
    {
      ret = new arithmetic_subshell_t(do_debashify_random(t->var, params));
    }
    else if(t->var != nullptr && t->var->index != nullptr)
    {
      ret = new arithmetic_subshell_t(do_debashify_array_var_get(t->var, params));
    }
  }
  return ret;
}

bool debashify_arithmetic_replace(_obj* o, debashify_params* params)
{
  bool ret=false;
  switch(o->type)
  {
    case _obj::subarg_arithmetic: {
      subarg_arithmetic_t* t = dynamic_cast<subarg_arithmetic_t*>(o);
      arithmetic_t* r = do_debashify_arithmetic_t(t->arith, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->arith;
        t->arith = r;
      }
    } break;
    case _obj::arithmetic_operation: {
      arithmetic_operation_t* t = dynamic_cast<arithmetic_operation_t*>(o);
      arithmetic_t* r = do_debashify_arithmetic_t(t->val1, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->val1;
        t->val1 = r;
      }
      r = do_debashify_arithmetic_t(t->val2, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->val2;
        t->val2 = r;
      }
    } break;
    case _obj::arithmetic_parenthesis: {
      arithmetic_parenthesis_t* t = dynamic_cast<arithmetic_parenthesis_t*>(o);
      arithmetic_t* r = do_debashify_arithmetic_t(t->val, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->val;
        t->val = r;
      }
    } break;
    default: break;
  }
  return ret;
}

bool debashify_subarg_replace(arg_t* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it=in->sa.begin() ; it!=in->sa.end() ; it++)
  {
    subarg_t* replacer=nullptr;
    bool quoted=(*it)->quoted;
    if((*it)->type == _obj::subarg_variable)
    {
      subarg_variable_t* t = dynamic_cast<subarg_variable_t*>(*it);
      if(t->var != nullptr && t->var->varname == "RANDOM")
      {
        replacer = new subarg_subshell_t(do_debashify_random(t->var, params));
      }
      if(t->var != nullptr && t->var->is_manip && t->var->index != nullptr)
      {
        replacer = new subarg_subshell_t(do_debashify_array_var_get(t->var, params));
      }
    }
    if(replacer != nullptr)
    {
      replacer->quoted=quoted;
      delete *it;
      *it = replacer;
      has_replaced=true;
    }
  }
  return has_replaced;
}

bool debashify_array_set(cmd_t* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it = in->var_assigns.begin() ; it != in->var_assigns.end() ; it++)
  {
    if(it->first!=nullptr && it->second != nullptr && it->second->size()>0 && it->second->first_sa_string().substr(0,2) == "=(")
    {
      // array creation: VAR=()
      // extract arguments from =(ARGS...)
      std::string gen=it->second->generate(0);
      std::string varname=it->first->varname;
      gen=gen.substr(2);
      gen.pop_back();
      // create cmd out of arguments
      arglist_t* args = parse_arglist( make_context(gen) ).first;
      cmd_t* c = new cmd_t(args);
      // cmd first argument is _lxsh_X_create
      if(params->arrays[varname])
      {
        c->args->insert(0, new arg_t("_lxsh_map_create") );
        params->require_fct("_lxsh_map_create");
      }
      else
      {
        c->args->insert(0, new arg_t("_lxsh_array_create") );
        params->require_fct("_lxsh_array_create");
      }
      subarg_subshell_t* sb = new subarg_subshell_t(new subshell_t(c));
      // insert new value
      delete it->second;
      it->second = new arg_t("=");
      it->second->add(sb);
      has_replaced=true;
    }
    else if(it->first != nullptr && it->first->index != nullptr)
    {
      // array value set: VAR[]=
      force_quotes(it->second);
      force_quotes(it->first->index);
      subarg_string_t* tt=dynamic_cast<subarg_string_t*>(it->second->sa[0]);

      std::string varname = it->first->varname;
      arg_t* index = it->first->index;
      arg_t* value = it->second;

      it->first->index = nullptr;
      it->second = nullptr;

      if(tt->val.substr(0,2) == "+=")
      {
        tt->val = tt->val.substr(2); // remove +=

        // create array get of value
        cmd_t* c;
        if(params->arrays[varname])
        {
          c = make_cmd_varindex("_lxsh_map_get", varname, copy(index));
          params->require_fct("_lxsh_map_get");
        }
        else
        {
          c = make_cmd_varindex("_lxsh_array_get", varname, copy(index));
          params->require_fct("_lxsh_array_get");
        }
        subarg_subshell_t* sb = new subarg_subshell_t(new subshell_t(c));
        sb->quoted=true;
        value->insert(0, "\"");
        value->insert(0, sb);
        value->insert(0, "\"");

      }
      else
        tt->val = tt->val.substr(1); // remove =

      cmd_t* c = new cmd_t(new arglist_t);
      if(params->arrays[varname])
      {
        c->args->add(new arg_t("_lxsh_map_set") );
        params->require_fct("_lxsh_map_set");
      }
      else
      {
        c->args->add(new arg_t("_lxsh_array_set") );
        params->require_fct("_lxsh_array_set");
      }
      // _lxsh_array_set "$VAR"
      c->args->add( make_arg("\"$"+varname+"\"") );
      // _lxsh_array_set "$VAR" N
      c->args->add( index );
      // _lxsh_array_set "$VAR" N value
      c->args->add( value );
      // $(_lxsh_array_set "$VAR" N value)
      subarg_subshell_t* sb = new subarg_subshell_t(new subshell_t(c));

      it->second = new arg_t("=");
      it->second->add(sb);
      has_replaced=true;
    }
    else if(it->first != nullptr && it->second!=nullptr && it->second->first_sa_string().substr(0,3) == "+=(")
    {
      // array add: VAR+=()
      // can be done by creating a new array with old array + new

      std::string varname = it->first->varname;

      // extract arguments from =+(ARGS...)
      std::string gen=it->second->generate(0);
      gen=gen.substr(3);
      gen.pop_back();
      // create cmd out of arguments
      arglist_t* args = parse_arglist( make_context(gen) ).first;
      cmd_t* c = new cmd_t(args);
      // cmd first argument is _lxsh_array_create
      if(params->arrays[varname])
      {
        throw std::runtime_error("Cannot debashify VAR+=() on associative arrays");
      }
      else
      {
        c->args->insert(0, new arg_t("_lxsh_array_create") );
        params->require_fct("_lxsh_array_create");
      }
      // second arg is varname
      c->args->insert(1, make_arg("\"$"+varname+"\"") );
      subarg_subshell_t* sb = new subarg_subshell_t(new subshell_t(c));
      // insert new value
      delete it->second;
      it->second = new arg_t("=");
      it->second->add(sb);
      has_replaced=true;
    }
  }
  return has_replaced;
}

bool debashify_plusequal(cmd_t* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it = in->var_assigns.begin() ; it != in->var_assigns.end() ; it++)
  {
    if(it->first != nullptr && it->second != nullptr && it->second->first_sa_string().substr(0,2) == "+=")
    {
      subarg_string_t* tt=dynamic_cast<subarg_string_t*>(it->second->sa[0]);
      variable_t* v = new variable_t(it->first->varname);
      v->is_manip=true;
      tt->val = tt->val.substr(2); // remove +=
      it->second->insert(0, new subarg_variable_t(v) );
      it->second->insert(0, "=");
    }
  }
  return has_replaced;
}

// replace <<< foo by printf %s\n "foo" |
bool debashify_herestring(pipeline_t* pl)
{
  if(pl->cmds.size()>0)
  {
    block_t* c=pl->cmds[0];
    for(uint32_t i=0; i<c->redirs.size() ; i++)
    {
      if(c->redirs[i]->op == "<<<")
      {
        force_quotes(c->redirs[i]->target);
        cmd_t* printcmd = make_cmd("printf %s\\\\n");
        printcmd->add(c->redirs[i]->target);
        pl->cmds.insert(pl->cmds.begin(), printcmd);

        // cleanup
        c->redirs[i]->target=nullptr;
        delete c->redirs[i];
        c->redirs.erase(pl->cmds[1]->redirs.begin()+i);

        return true;
      }
    }
  }
  return false;
}

// replace &>, &>> and >&:
// add 2>&1 as redirect
bool debashify_combined_redirects(block_t* in)
{
  bool has_replaced=false;

  for(uint32_t i=0; i<in->redirs.size() ; i++)
  {
    if(in->redirs[i]->op == "&>" || in->redirs[i]->op == "&>>" || in->redirs[i]->op == ">&")
    {
      // resolve new operator
      std::string newop = ">";
      if( in->redirs[i]->op == "&>>" )
        newop = ">>";
      // create new redir with target
      redirect_t* newredir = new redirect_t(newop, in->redirs[i]->target);
      in->redirs[i]->target=nullptr;
      // replace old redir
      delete in->redirs[i];
      in->redirs[i] = newredir;
      // insert merge redir
      i++;
      in->redirs.insert(in->redirs.begin()+i, new redirect_t("2>&1"));

      has_replaced=true;
    }
  }

  return has_replaced;
}

// replace <() and >()
/*
REPLACE TO:
  fifoN=${TMPDIR-/tmp}/lxshfifo_$(_lxsh_random_string 10)
  mkfifo "$fifoN"
  ( {PSUB;} [>|<] "$fifoN" ; rm "$fifoN") &
  CMD "$fifoN"
*/
bool debashify_procsub(list_t* lst, debashify_params* params)
{
  bool has_replaced=false;
  for(uint32_t li=0; li<lst->cls.size(); li++)
  {
    std::vector<std::pair<arg_t*,bool>> affected_args;
    // iterate all applicable args of the cl
    for(auto plit: lst->cls[li]->pls)
    {
      for(auto cmit: plit->cmds)
      {
        if(cmit->type == _obj::block_cmd)
        {
          cmd_t* t = dynamic_cast<cmd_t*>(cmit);
          if(t->args != nullptr)
          {
            for(auto ait: t->args->args)
            {
              if(ait->size() == 1 && ait->sa[0]->type == _obj::subarg_procsub)
              {
                subarg_procsub_t* st = dynamic_cast<subarg_procsub_t*>(ait->sa[0]);
                affected_args.push_back( std::make_pair(ait, st->is_output) );
              }
            }
          }
        }
      }
    }
    // perform the replace
    if(affected_args.size()>0)
    {
      params->require_fct("_lxsh_random_tmpfile");
      has_replaced=true;
      list_t* lst_insert = new list_t;
      std::string mkfifocmd="mkfifo";
      for(uint32_t i=0; i<affected_args.size(); i++)
      {
        // fifoN=${TMPDIR-/tmp}/lxshfifo_$(_lxsh_random_string 10)
        lst_insert->add( make_condlist( strf("_lxshfifo%u=$(_lxsh_random_tmpfile lxshfifo)", i) ) );
        mkfifocmd += strf(" \"$_lxshfifo%u\"", i);
      }
      // mkfifo "$fifoN"
      lst_insert->add( make_condlist(mkfifocmd) );
      for(uint32_t i=0; i<affected_args.size(); i++)
      {
        // create ( {PSUB;} > "$fifoN" ; rm "$fifoN") &
        subshell_t* psub = new subshell_t(new list_t);
        subarg_procsub_t* st = dynamic_cast<subarg_procsub_t*>(affected_args[i].first->sa[0]);
        // {PSUB;}
        brace_t* cbr = new brace_t(st->sbsh->lst);
        // deindex list for delete
        st->sbsh->lst=nullptr;
        // {PSUB;} > "$_lxshfifoN"
        cbr->redirs.push_back( new redirect_t( affected_args[i].second ? "<" : ">", make_arg(strf("\"$_lxshfifo%u\"", i)) ) );
        // ( {PSUB;} > "$_lxshfifoN" )
        psub->lst->add( new condlist_t(cbr) );
        // ( {PSUB;} > "$_lxshfifoN" ; rm "$_lxshfifoN" )
        psub->lst->add( make_condlist(strf("rm \"$_lxshfifo%u\"", i)) );
        // ( {PSUB;} > "$_lxshfifoN" ; rm "$_lxshfifoN" ) &
        condlist_t* pscl = new condlist_t(psub);
        pscl->parallel=true;
        lst_insert->add( pscl );

        // replace the arg
        delete affected_args[i].first->sa[0];
        affected_args[i].first->sa[0] = new subarg_string_t("\"");
        affected_args[i].first->add( new subarg_variable_t( new variable_t(strf("_lxshfifo%u", i)) ) );
        affected_args[i].first->add( "\"" );
      }
      lst->insert(li, *lst_insert );
      li+= lst_insert->size();
      //cleanup
      lst_insert->cls.resize(0);
      delete lst_insert;
    }
  }
  return has_replaced;
}

condlist_t* debashify_manipulation_substring(variable_t* v, debashify_params* params)
{
  subarg_string_t* first = dynamic_cast<subarg_string_t*>(v->manip->sa[0]);
  first->val = first->val.substr(1);
  if(first->val == "")
  {
    delete v->manip->sa[0];
    v->manip->sa.erase(v->manip->sa.begin());
  }
  std::string manip = v->manip->first_sa_string();
  arg_t *arg1=nullptr, *arg2=nullptr;
  size_t colon_pos = manip.find(':');
  if(colon_pos != std::string::npos || v->manip->sa.size()>1)
  {
    for(uint32_t i=0; i<v->manip->sa.size(); i++)
    {
      if(v->manip->sa[i]->type == _obj::subarg_string)
      {
        subarg_string_t* t = dynamic_cast<subarg_string_t*>(v->manip->sa[i]);
        size_t colon_pos = t->val.find(':');
        if(colon_pos != std::string::npos)
        {
          arg1 = new arg_t;
          arg2 = new arg_t;
          for(uint32_t j=0; j<i; j++)
            arg1->add(v->manip->sa[j]);
          std::string val=t->val.substr(0, colon_pos);
          if(val != "")
            arg1->add(val);
          val=t->val.substr(colon_pos+1);
          if(val != "")
            arg2->add(val);
          for(uint32_t j=i+1; j<v->manip->sa.size(); j++)
            arg2->add(v->manip->sa[j]);
          delete v->manip->sa[i];
          v->manip->sa.resize(0);
          break;
          // TODO
        }
      }
    }
    if(arg1 == nullptr)
    {
      arg1 = v->manip;
      v->manip = nullptr;
    }
  }
  else
  {
    arg1 = v->manip;
    v->manip = nullptr;
  }

  pipeline_t* pl = new pipeline_t(make_printf_variable(v->varname));
  arg_t* retarg = new arg_t;
  retarg->add(new subarg_arithmetic_t(make_arithmetic(arg1, "+", new arg_t("1"))));
  retarg->add("-");
  pl->add(make_cmd({new arg_t("cut"), new arg_t("-c"), retarg}));

  if(arg2 != nullptr)
  {
    retarg = new arg_t;
    retarg->add("-");
    for(auto it: arg2->sa)
    {
      retarg->add(it);
    }
    arg2->sa.resize(0);
    delete arg2;
    arg2 = nullptr;
    pl->add(make_cmd({new arg_t("cut"), new arg_t("-c"), retarg}));
  }

  if(v->manip != nullptr)
    delete v->manip;
  v->manip = nullptr;

  return new condlist_t(pl);
}

bool debashify_manipulation(arg_t* in, debashify_params* params)
{
  bool has_replaced=false;
  for(uint32_t i=0; i<in->sa.size(); i++)
  {
    if(in->sa[i]->type == _obj::subarg_variable)
    {
      variable_t* v = dynamic_cast<subarg_variable_t*>(in->sa[i])->var;
      if(!v->is_manip || v->manip == nullptr)
        return false;
      std::string manip = v->manip->first_sa_string();
      subarg_t* r = nullptr;
      if(v->is_manip && v->precedence && v->manip->string() == "!")
      {
        arg_t* eval_arg = new arg_t;
        eval_arg->add("\\\"\\${");
        eval_arg->add(new subarg_variable_t(new variable_t(v->varname)));
        eval_arg->add("}\\\"");
        cmd_t* eval_cmd = make_cmd({new arg_t("eval"), new arg_t("echo"), eval_arg});
        r = new subarg_subshell_t(new subshell_t(eval_cmd));
      }
      else if(manip.size()>0 && manip[0] == '/')
      {
        cmd_t* prnt = make_printf_variable(v->varname);
        // printf %s\\n "$var"
        cmd_t* sed = make_cmd({std::string("sed")});
        arg_t* sedarg=v->manip;
        v->manip = nullptr;
        sedarg->insert(0, "s");
        sedarg->add("/");
        force_quotes(sedarg);
        sed->add(sedarg);
        // sed "s///g"
        pipeline_t* pl = new pipeline_t(prnt);
        pl->add(sed);
        r = new subarg_subshell_t(new subshell_t(new list_t(new condlist_t(pl))));
      }
      else if(manip.size()>0 && manip[0] == ':' && !(manip.size()>1 && is_in(manip[1], "+-") ) )
      {
        r = new subarg_subshell_t(new subshell_t(new list_t(debashify_manipulation_substring(v, params))));
      }

      if(r != nullptr)
      {
        r->quoted = in->sa[i]->quoted;
        delete in->sa[i];
        in->sa[i] = r;
        has_replaced=true;
      }
    }
  }
  return has_replaced;
}

uint32_t n_zerolead(std::string in)
{
  if(in == "0")
    return 0;
  uint32_t lead=0;
  while(in[lead] == '0')
    lead++;
  return lead;
}

bool debashify_brace_expansion(arglist_t* in, debashify_params* params)
{
  bool has_replaced=false;
  start:
  for(uint32_t iarg=0; iarg<=in->args.size(); iarg++)
  {
    // don't treat non-pure-string arguments for now
    if(in->args[iarg] == nullptr || in->args[iarg]->sa.size() != 1 || in->args[iarg]->sa[0]->type != _obj::subarg_string)
      continue;

    std::string& val = dynamic_cast<subarg_string_t*>(in->args[iarg]->sa[0])->val;

    uint32_t i=0, start=0;
    while(i<val.size())
    {
      switch(val[i]) {
        case '{' : {
          start=i;
          i++;
          size_t end=i;
          uint32_t counter=0;
          while(end < val.size() && ( counter != 0 || val[end] != '}') ) {
            if(val[end] == '{')
              counter++;
            if(val[end] == '}')
              counter--;
            end++;
          }
          if(end >= val.size())
            continue;

          std::string s = val.substr(i, end-i);

          std::vector<std::string> values;
          if(s.find(',') != std::string::npos)
          {
            size_t l;
            do
            {
              l = s.find(',');
              values.push_back(s.substr(0, l));
              s = s.substr(l+1);
            } while(l != std::string::npos);
          }
          else if(s.find("..") != std::string::npos)
          {
            size_t l = s.find("..");


            int64_t inc=1;
            std::string val1, val2, val3;
            val1 = s.substr(0, l);
            val2 = s.substr(l+2);
            if( (l = val2.find("..")) != std::string::npos) {
              val3 = val2.substr(l+2);
              val2 = val2.substr(0, l);
            }

            if(val3.size()>0) {
              try {
                inc = std::stol(val3);
                if(inc < 0)
                  inc *= -1;
              }
              catch(std::invalid_argument& e) {
                continue;
              }
            }

            int64_t seqstart, seqend;
            uint32_t nlead=0;
            bool ischar=false;

            try {
              seqstart = std::stol(val1);
              seqend = std::stol(val2);
              nlead = std::max(n_zerolead(val1), n_zerolead(val2));
            }
            catch(std::invalid_argument& e) {
              if(val1.size()!=1 || val2.size()!=1)
                continue;
              ischar=true;
              seqstart = val1[0];
              seqend = val2[0];
            }

            if(seqend < seqstart) {
              int64_t tint = seqend;
              seqend = seqstart;
              seqstart = tint;
            }

            for(int64_t ii = seqstart; ii <= seqend; ii += inc) {
              if(ischar)
                values.push_back( std::string(1, (char) ii) );
              else
                values.push_back(strf("%0*d", nlead+1, ii));
            }

          }

          if(values.size()>0) {
            for (unsigned n = values.size(); n-- > 0; ) {
              in->args.insert(in->args.begin()+iarg+1, new arg_t(val.substr(0, start) + values[n] + val.substr(end+1)));
            }
            in->args.erase(in->args.begin()+iarg);
            has_replaced=true;
            goto start;
          }

        } break;
        case '\'': {
          i++;
          while(val[i] != '\'')
            i++;
        } break;
        case '"' : {
          i++;
          while(val[i] != '"') {
            if(val[i] == '\\')
              i++;
            i++;
          }
        } break;
        default: i++;
      }
    }
  }
  return has_replaced;
}

bool debashify_var(variable_t* in, debashify_params* params)
{
  return false;
}

bool r_debashify(_obj* o, debashify_params* params)
{
  // global debashifies
  debashify_arithmetic_replace(o, params);
  switch(o->type)
  {
    case _obj::variable: {
      variable_t* t = dynamic_cast<variable_t*>(o);
      debashify_var(t, params);
    } break;
    case _obj::arg: {
      arg_t* t = dynamic_cast<arg_t*>(o);
      debashify_subarg_replace(t, params);
      debashify_manipulation(t, params);
    } break;
    case _obj::arglist: {
      arglist_t* t = dynamic_cast<arglist_t*>(o);
      debashify_brace_expansion(t, params);
    } break;
    case _obj::list: {
      list_t* t = dynamic_cast<list_t*>(o);
      debashify_declare(t, params);
      debashify_readonly(t);
      debashify_procsub(t, params);
    } break;
    case _obj::pipeline: {
      pipeline_t* t = dynamic_cast<pipeline_t*>(o);
      debashify_echo(t);
      debashify_herestring(t);
      debashify_bashtest(t);
    } break;
    case _obj::block_cmd: {
      cmd_t* t = dynamic_cast<cmd_t*>(o);
      debashify_combined_redirects(t);
      debashify_array_set(t, params);
      debashify_plusequal(t, params);
    } break;
    case _obj::block_subshell: {
      subshell_t* t = dynamic_cast<subshell_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_brace: {
      brace_t* t = dynamic_cast<brace_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_main: {
      shmain* t = dynamic_cast<shmain*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_function: {
      function_t* t = dynamic_cast<function_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_case: {
      case_t* t = dynamic_cast<case_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_if: {
      if_t* t = dynamic_cast<if_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_while: {
      while_t* t = dynamic_cast<while_t*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_for: {
      for_t* t = dynamic_cast<for_t*>(o);
      debashify_combined_redirects(t);
    } break;
    default: break;
  }
  return true;
}

// return value: dependencies
std::set<std::string> debashify(_obj* o, debashify_params* params)
{
  recurse(r_debashify, o, params);
  return params->required_fcts;
}

// return value: dependencies
std::set<std::string> debashify(shmain* sh)
{
  debashify_params params;
  sh->shebang = "#!/bin/sh";
  recurse(r_debashify, sh, &params);
  return params.required_fcts;
}
