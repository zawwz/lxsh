#include "debashify.hpp"

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
block* gen_bashtest_cmd(std::vector<arg*> args)
{
  block* ret = nullptr;

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
    args[1] = new arg(arg1replace);
  }

  if(args.size() == 3 && args[1]->string() == "=" && (arg_has_char('*', args[2]) || arg_has_char('?', args[2])) )
  {
    // glob matcher: do a case
    delete args[1];
    args[1]=nullptr;
    case_block* tc = new case_block(args[0]);
    tc->cases.push_back( std::make_pair(std::vector<arg*>({args[2]}), make_list("true")) );
    tc->cases.push_back( std::make_pair(std::vector<arg*>({new arg("*")}), make_list("false")) );
    ret = tc;
  }
  else if(args.size() == 3 && args[1]->string() == "=~")
  {
    // regex matcher: use expr
    delete args[1];
    args[1]=nullptr;
    args[2]->insert(0, ".*");
    add_quotes(args[2]);
    ret = make_cmd( std::vector<arg*>({ new arg("expr"), args[0], new arg(":"), args[2] }) );
    ret->redirs.push_back(new redirect(">", new arg("/dev/null") ));
  }
  else
  {
    // regular [ ]
    cmd* t = make_cmd(args);
    t->args->insert(0, new arg("["));
    t->add(new arg("]"));
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
bool debashify_bashtest(pipeline* pl)
{
  if(pl->cmds.size()<=0)
    return false;

  if(pl->cmds[0]->type != _obj::block_cmd)
    return false;
  cmd* in = dynamic_cast<cmd*>(pl->cmds[0]);

  if(in->arg_string(0) == "[[")
  {
    brace* br = new brace(new list);
    condlist* cl = new condlist;
    br->lst->add(cl);

    arg *a=nullptr;
    uint32_t j=1;
    bool or_op=false;
    for(uint32_t i=1 ; i<in->args->size() ; i++)
    {
      a = in->args->args[i];

      if(i >= in->args->size()-1 || a->string() == "&&" || a->string() == "||")
      {
        block* tbl = gen_bashtest_cmd(std::vector<arg*>(in->args->args.begin()+j, in->args->args.begin()+i));
        cl->add(new pipeline(tbl), or_op);
        or_op = a->string() == "||";
        j=i+1;

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

std::string get_declare_opt(cmd* in)
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

bool debashify_echo(pipeline* pl)
{
  if(pl->cmds[0]->type != _obj::block_cmd)
    return false;
  cmd* in = dynamic_cast<cmd*>(pl->cmds[0]);

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

        in->args->insert(1, new arg(format_str));
        delete in->args->args[0];
        in->args->args[0] = new arg("printf");
      }
      else
      {
        std::string format_str;
        if(enable_interpretation)
          format_str = "%b";
        else
          format_str = "%s";

        list* lst=nullptr;

        // more than 1 arg and first arg can't expand: can split into two printf
        // printf '%s' arg1
        // printf ' %s' args...
        if(in->args->args.size()>2 && !in->args->args[1]->can_expand())
        {
          // extract arg 1
          arg* arg1 = in->args->args[1];
          in->args->args.erase(in->args->args.begin()+1);
          delete in->args->args[0];
          in->args->args[0] = new arg("printf");

          lst = new list;
          lst->add(new condlist(make_cmd({new arg("printf"), new arg(format_str), arg1 })));
          in->args->insert(1, new arg("\\ "+format_str) );
          lst->add(new condlist(in));
        }
        else
        {
          // can't reliable replace: keep echo if newline
          if(newline)
            return has_processed_options;

          in->args->insert(1, new arg(format_str+"\\ "));
          delete in->args->args[0];
          in->args->args[0] = new arg("printf");
        }

        if(newline)
        {
          if(lst == nullptr)
          {
            lst = new list;
            lst->add(new condlist(in));
          }
          lst->add(make_condlist("echo"));
        }

        if(lst != nullptr)
        {
          pl->cmds[0] = new brace(lst);
        }
      }
    }

    return true;
  }
  return false;
}

bool debashify_readonly(list* in)
{
  bool has_found=false;
  for(uint32_t i=0; i<in->cls.size(); i++)
  {
    // not a cmd: go to next
    if(in->cls[i]->pls[0]->cmds[0]->type != _obj::block_cmd)
      continue;

    cmd* c1 = dynamic_cast<cmd*>(in->cls[i]->pls[0]->cmds[0]);
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
        c1->args = new arglist;
        c1->var_assigns=c1->cmd_var_assigns;
        c1->cmd_var_assigns.resize(0);
      }
    }
  }
  if(in->cls.size()<=0)
    in->add(make_condlist("true"));
  return has_found;
}

bool debashify_declare(list* in, debashify_params* params)
{
  bool has_found=false;
  for(uint32_t i=0; i<in->cls.size(); i++)
  {
    // not a cmd: go to next
    if(in->cls[i]->pls[0]->cmds[0]->type != _obj::block_cmd)
      continue;

    cmd* c1 = dynamic_cast<cmd*>(in->cls[i]->pls[0]->cmds[0]);
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

cmd* make_cmd_varindex(std::string const& strcmd, std::string const& varname, arg* index)
{
  cmd* c = new cmd(new arglist);
  // cmd
  c->args->add( new arg(strcmd) );
  // cmd "$VAR"
  c->args->add( make_arg("\"$"+varname+"\"") );
  // cmd "$VAR" N
  c->args->add( index );
  return c;
}

subshell* do_debashify_array_var_get(variable* in, debashify_params* params)
{
  if(in->manip != nullptr)
    throw std::runtime_error("Cannot debashify manipulations on ${VAR[]}");

  std::string varname = in->varname;
  arg* index = in->index;
  in->index=nullptr;

  if(index->string() == "*")
  {
    delete index;
    index = new arg("\\*");
  }

  cmd* c;
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

  return new subshell(c);
}

subshell* do_debashify_random(variable* in, debashify_params* params)
{
  if(in->manip != nullptr)
    throw std::runtime_error("Cannot debashify manipulations on ${RANDOM}");
  cmd* c = make_cmd("_lxsh_random");
  params->require_fct("_lxsh_random");
  return new subshell(c);
}

// does multiple debashifies:
// - array
// - RANDOM
subshell_arithmetic* do_debashify_arithmetic(arithmetic* in, debashify_params* params)
{
  subshell_arithmetic* ret = nullptr;
  if(in->type == _obj::arithmetic_variable)
  {
    variable_arithmetic* t = dynamic_cast<variable_arithmetic*>(in);
    if(t->var != nullptr && t->var->varname == "RANDOM")
    {
      ret = new subshell_arithmetic(do_debashify_random(t->var, params));
    }
    else if(t->var != nullptr && t->var->index != nullptr)
    {
      ret = new subshell_arithmetic(do_debashify_array_var_get(t->var, params));
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
      arithmetic_subarg* t = dynamic_cast<arithmetic_subarg*>(o);
      arithmetic* r = do_debashify_arithmetic(t->arith, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->arith;
        t->arith = r;
      }
    } break;
    case _obj::arithmetic_operation: {
      operation_arithmetic* t = dynamic_cast<operation_arithmetic*>(o);
      arithmetic* r = do_debashify_arithmetic(t->val1, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->val1;
        t->val1 = r;
      }
      r = do_debashify_arithmetic(t->val2, params);
      if(r!=nullptr)
      {
        ret=true;
        delete t->val2;
        t->val2 = r;
      }
    } break;
    case _obj::arithmetic_parenthesis: {
      parenthesis_arithmetic* t = dynamic_cast<parenthesis_arithmetic*>(o);
      arithmetic* r = do_debashify_arithmetic(t->val, params);
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

bool debashify_subarg_replace(arg* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it=in->sa.begin() ; it!=in->sa.end() ; it++)
  {
    subarg* replacer=nullptr;
    bool quoted=(*it)->quoted;
    if((*it)->type == _obj::subarg_variable)
    {
      variable_subarg* t = dynamic_cast<variable_subarg*>(*it);
      if(t->var != nullptr && t->var->varname == "RANDOM")
      {
        replacer = new subshell_subarg(do_debashify_random(t->var, params));
      }
      if(t->var != nullptr && t->var->is_manip && t->var->index != nullptr)
      {
        replacer = new subshell_subarg(do_debashify_array_var_get(t->var, params));
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

bool debashify_array_set(cmd* in, debashify_params* params)
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
      arglist* args = parse_arglist( make_context(gen) ).first;
      cmd* c = new cmd(args);
      // cmd first argument is _lxsh_X_create
      if(params->arrays[varname])
      {
        c->args->insert(0, new arg("_lxsh_map_create") );
        params->require_fct("_lxsh_map_create");
      }
      else
      {
        c->args->insert(0, new arg("_lxsh_array_create") );
        params->require_fct("_lxsh_array_create");
      }
      subshell_subarg* sb = new subshell_subarg(new subshell(c));
      // insert new value
      delete it->second;
      it->second = new arg("=");
      it->second->add(sb);
      has_replaced=true;
    }
    else if(it->first != nullptr && it->first->index != nullptr)
    {
      // array value set: VAR[]=
      force_quotes(it->second);
      force_quotes(it->first->index);
      string_subarg* tt=dynamic_cast<string_subarg*>(it->second->sa[0]);

      std::string varname = it->first->varname;
      arg* index = it->first->index;
      arg* value = it->second;

      it->first->index = nullptr;
      it->second = nullptr;

      if(tt->val.substr(0,2) == "+=")
      {
        tt->val = tt->val.substr(2); // remove +=

        // create array get of value
        cmd* c;
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
        subshell_subarg* sb = new subshell_subarg(new subshell(c));
        sb->quoted=true;
        value->insert(0, "\"");
        value->insert(0, sb);
        value->insert(0, "\"");

      }
      else
        tt->val = tt->val.substr(1); // remove =

      cmd* c = new cmd(new arglist);
      if(params->arrays[varname])
      {
        c->args->add(new arg("_lxsh_map_set") );
        params->require_fct("_lxsh_map_set");
      }
      else
      {
        c->args->add(new arg("_lxsh_array_set") );
        params->require_fct("_lxsh_array_set");
      }
      // _lxsh_array_set "$VAR"
      c->args->add( make_arg("\"$"+varname+"\"") );
      // _lxsh_array_set "$VAR" N
      c->args->add( index );
      // _lxsh_array_set "$VAR" N value
      c->args->add( value );
      // $(_lxsh_array_set "$VAR" N value)
      subshell_subarg* sb = new subshell_subarg(new subshell(c));

      it->second = new arg("=");
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
      arglist* args = parse_arglist( make_context(gen) ).first;
      cmd* c = new cmd(args);
      // cmd first argument is _lxsh_array_create
      if(params->arrays[varname])
      {
        throw std::runtime_error("Cannot debashify VAR+=() on associative arrays");
      }
      else
      {
        c->args->insert(0, new arg("_lxsh_array_create") );
        params->require_fct("_lxsh_array_create");
      }
      // second arg is varname
      c->args->insert(1, make_arg("\"$"+varname+"\"") );
      subshell_subarg* sb = new subshell_subarg(new subshell(c));
      // insert new value
      delete it->second;
      it->second = new arg("=");
      it->second->add(sb);
      has_replaced=true;
    }
  }
  return has_replaced;
}

bool debashify_plusequal(cmd* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it = in->var_assigns.begin() ; it != in->var_assigns.end() ; it++)
  {
    if(it->first != nullptr && it->second != nullptr && it->second->first_sa_string().substr(0,2) == "+=")
    {
      string_subarg* tt=dynamic_cast<string_subarg*>(it->second->sa[0]);
      variable* v = new variable(it->first->varname);
      v->is_manip=true;
      tt->val = tt->val.substr(2); // remove +=
      it->second->insert(0, new variable_subarg(v) );
      it->second->insert(0, "=");
    }
  }
  return has_replaced;
}

// replace <<< foo by printf %s\n "foo" |
bool debashify_herestring(pipeline* pl)
{
  if(pl->cmds.size()>0)
  {
    block* c=pl->cmds[0];
    for(uint32_t i=0; i<c->redirs.size() ; i++)
    {
      if(c->redirs[i]->op == "<<<")
      {
        force_quotes(c->redirs[i]->target);
        cmd* printcmd = make_cmd("printf %s\\\\n");
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
bool debashify_combined_redirects(block* in)
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
      redirect* newredir = new redirect(newop, in->redirs[i]->target);
      in->redirs[i]->target=nullptr;
      // replace old redir
      delete in->redirs[i];
      in->redirs[i] = newredir;
      // insert merge redir
      i++;
      in->redirs.insert(in->redirs.begin()+i, new redirect("2>&1"));

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
bool debashify_procsub(list* lst, debashify_params* params)
{
  bool has_replaced=false;
  for(uint32_t li=0; li<lst->cls.size(); li++)
  {
    std::vector<std::pair<arg*,bool>> affected_args;
    // iterate all applicable args of the cl
    for(auto plit: lst->cls[li]->pls)
    {
      for(auto cmit: plit->cmds)
      {
        if(cmit->type == _obj::block_cmd)
        {
          cmd* t = dynamic_cast<cmd*>(cmit);
          if(t->args != nullptr)
          {
            for(auto ait: t->args->args)
            {
              if(ait->size() == 1 && ait->sa[0]->type == _obj::subarg_procsub)
              {
                procsub_subarg* st = dynamic_cast<procsub_subarg*>(ait->sa[0]);
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
      list* lst_insert = new list;
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
        subshell* psub = new subshell(new list);
        procsub_subarg* st = dynamic_cast<procsub_subarg*>(affected_args[i].first->sa[0]);
        // {PSUB;}
        brace* cbr = new brace(st->sbsh->lst);
        // deindex list for delete
        st->sbsh->lst=nullptr;
        // {PSUB;} > "$_lxshfifoN"
        cbr->redirs.push_back( new redirect( affected_args[i].second ? "<" : ">", make_arg(strf("\"$_lxshfifo%u\"", i)) ) );
        // ( {PSUB;} > "$_lxshfifoN" )
        psub->lst->add( new condlist(cbr) );
        // ( {PSUB;} > "$_lxshfifoN" ; rm "$_lxshfifoN" )
        psub->lst->add( make_condlist(strf("rm \"$_lxshfifo%u\"", i)) );
        // ( {PSUB;} > "$_lxshfifoN" ; rm "$_lxshfifoN" ) &
        condlist* pscl = new condlist(psub);
        pscl->parallel=true;
        lst_insert->add( pscl );

        // replace the arg
        delete affected_args[i].first->sa[0];
        affected_args[i].first->sa[0] = new string_subarg("\"");
        affected_args[i].first->add( new variable_subarg( new variable(strf("_lxshfifo%u", i)) ) );
        affected_args[i].first->add( new string_subarg("\"") );
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

bool debashify_variable_substitution(arg* in, debashify_params* params)
{
  bool has_replaced=false;
  for(uint32_t i=0; i<in->sa.size(); i++)
  {
    if(in->sa[i]->type == _obj::subarg_variable)
    {
      variable* v = dynamic_cast<variable_subarg*>(in->sa[i])->var;
      if(v->is_manip && v->precedence && v->manip->string() == "!")
      {
        arg* eval_arg = new arg;
        eval_arg->add(new string_subarg("echo \\\"\\${"));
        eval_arg->add(new variable_subarg(new variable(v->varname)));
        eval_arg->add(new string_subarg("}\\\""));
        cmd* eval_cmd = make_cmd(std::vector<arg*>({new arg("eval"), eval_arg}));
        subshell_subarg* r = new subshell_subarg(new subshell(eval_cmd));
        r->quoted = in->sa[i]->quoted;
        delete in->sa[i];
        in->sa[i] = r;
        has_replaced=true;
      }
    }
  }
  return has_replaced;
}

bool debashify_var(variable* in, debashify_params* params)
{
  return false;
}

bool r_debashify(_obj* o, debashify_params* params)
{
  // global debashifies
  debashify_arithmetic_replace(o, params);
  switch(o->type)
  {
    case _obj::_variable: {
      variable* t = dynamic_cast<variable*>(o);
      debashify_var(t, params);
    } break;
    case _obj::_arg: {
      arg* t = dynamic_cast<arg*>(o);
      debashify_subarg_replace(t, params);
      debashify_variable_substitution(t, params);
    } break;
    case _obj::_list: {
      list* t = dynamic_cast<list*>(o);
      debashify_declare(t, params);
      debashify_readonly(t);
      debashify_procsub(t, params);
    } break;
    case _obj::_pipeline: {
      pipeline* t = dynamic_cast<pipeline*>(o);
      debashify_echo(t);
      debashify_herestring(t);
      debashify_bashtest(t);
    } break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(o);
      debashify_combined_redirects(t);
      debashify_array_set(t, params);
      debashify_plusequal(t, params);
    } break;
    case _obj::block_subshell: {
      subshell* t = dynamic_cast<subshell*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_brace: {
      brace* t = dynamic_cast<brace*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_main: {
      shmain* t = dynamic_cast<shmain*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_function: {
      function* t = dynamic_cast<function*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_case: {
      case_block* t = dynamic_cast<case_block*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_if: {
      if_block* t = dynamic_cast<if_block*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_while: {
      while_block* t = dynamic_cast<while_block*>(o);
      debashify_combined_redirects(t);
    } break;
    case _obj::block_for: {
      for_block* t = dynamic_cast<for_block*>(o);
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
