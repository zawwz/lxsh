#include "debashify.hpp"

#include "ztd/options.hpp"

#include "processing.hpp"
#include "recursive.hpp"
#include "util.hpp"
#include "parse.hpp"
#include "struc_helper.hpp"

#include "g_shellcode.h"


typedef struct debashify_params {
  bool need_random_string=false;
  bool need_random_tmpfile=false;
  bool need_array_create=false;
  bool need_array_set=false;
  bool need_array_get=false;
  bool need_map_create=false;
  bool need_map_set=false;
  bool need_map_get=false;
  // map of detected arrays
  // bool value: is associative
  std::map<std::string,bool> arrays;
} debashify_params;


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
  if(in->var_assigns[0].second!=nullptr)
  {
    return in->var_assigns[0].second->string();
  }
  return "";
}

ztd::option_set gen_echo_opts()
{
  ztd::option_set ret;
  ret.add(
      ztd::option('e'),
      ztd::option('E'),
      ztd::option('n')
  );
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
      postargs=opts.process(args, true, true);
    }
    catch(ztd::option_error& e)
    {
      skip=true;
    }
    if(skip || postargs.size() == args.size()) // no options processed: skip
      return false;

    // delete the number of args that were processed
    for(uint32_t i=0; i<args.size()-postargs.size(); i++)
    {
      delete in->args->args[1];
      in->args->args.erase(in->args->args.begin()+1);
    }

    bool doprintf=false;
    bool newline=true;
    if(opts['E'])
    {
      doprintf=true;
    }
    else if(opts['n'])
    {
      doprintf=true;
      newline=false;
    }

    if(doprintf)
    {
      delete in->args->args[0];
      in->args->args[0] = new arg("printf");
      if(possibly_expands(in->args->args[2]) )
      {
        in->args->insert(1, new arg("%s\\ "));
        if(newline) // newline: add a newline command at the end
        {
          brace* br = new brace(new list);
          br->lst->add(new condlist(in));
          br->lst->add(make_condlist("echo"));
          pl->cmds[0] = br;
        }
      }
      else
      {
        std::string printfarg="'%s";
        for(uint32_t i=2; i<in->args->size(); i++)
          printfarg+=" %s";
        if(newline)
          printfarg+="\\n";
        printfarg+="'";
        in->args->insert(1, new arg(printfarg));
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
      for(uint32_t i=0; i<c1->var_assigns.size(); i++)
      {
        if(c1->var_assigns[i].first == nullptr || c1->var_assigns[i].second == nullptr)
        {
          if(c1->var_assigns[i].first != nullptr)
            delete c1->var_assigns[i].first;
          if(c1->var_assigns[i].second != nullptr)
            delete c1->var_assigns[i].second;
          c1->var_assigns.erase(c1->var_assigns.begin()+i);
          i--;
        }
      }
      if(c1->var_assigns.size() == 0)
      {
        delete in->cls[i];
        in->cls.erase(in->cls.begin()+i);
        i--;
      }
      else
      {
        delete c1->args;
        c1->args = new arglist;
      }
    }
  }
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
        for(auto it: c1->var_assigns)
        {
          if(it.first != nullptr)
          params->arrays[it.first->varname] = false;
        }
      }
      else if(op == "-A")
      {
        for(auto it: c1->var_assigns)
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

subshell_arithmetic* do_debashify_arithmetic(arithmetic* in, debashify_params* params)
{
  subshell_arithmetic* ret = nullptr;
  if(in->type == _obj::arithmetic_variable)
  {
    variable_arithmetic* t = dynamic_cast<variable_arithmetic*>(in);
    if(t->var != nullptr && t->var->index != nullptr)
    {
      if(t->var->manip != nullptr)
        throw std::runtime_error("Cannot debashify manipulations on ${VAR[]}");

      std::string varname = t->var->varname;
      arg* index = t->var->index;
      t->var->index=nullptr;

      cmd* c;
      if(params->arrays[varname])
      {
        c = make_cmd_varindex("__lxsh_map_get", varname, index);
        params->need_map_get=true;
      }
      else
      {
        c = make_cmd_varindex("__lxsh_array_get", varname, index);
        params->need_array_get=true;
      }

      ret = new subshell_arithmetic(new subshell(c));
    }
  }
  return ret;
}

bool debashify_array_arithmetic(_obj* o, debashify_params* params)
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

bool debashify_array_get(arg* in, debashify_params* params)
{
  bool has_replaced=false;
  for(auto it=in->sa.begin() ; it!=in->sa.end() ; it++)
  {
    if((*it)->type == _obj::subarg_variable)
    {
      variable_subarg* t = dynamic_cast<variable_subarg*>(*it);
      bool quoted=t->quoted;
      if(t->var != nullptr && t->var->is_manip && t->var->index != nullptr)
      {
        if(t->var->manip != nullptr)
          throw std::runtime_error("Cannot debashify manipulations on ${VAR[]}");

        std::string varname = t->var->varname;
        arg* index = t->var->index;
        t->var->index=nullptr;

        if(index->string() == "*")
        {
          delete index;
          index = new arg("\\*");
        }

        cmd* c;
        if(params->arrays[varname])
        {
          c = make_cmd_varindex("__lxsh_map_get", varname, index);
          params->need_map_get=true;
        }
        else
        {
          c = make_cmd_varindex("__lxsh_array_get", varname, index);
          params->need_array_get=true;
        }

        subshell_subarg* sb = new subshell_subarg(new subshell(c));
        sb->quoted=quoted;
        delete *it;
        *it = sb;
        has_replaced=true;
      }
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
      arglist* args = parse_arglist( gen.c_str(), gen.size(), 0 ).first;
      cmd* c = new cmd(args);
      // cmd first argument is __lxsh_X_create
      if(params->arrays[varname])
      {
        c->args->insert(0, new arg("__lxsh_map_create") );
        params->need_map_create=true;
      }
      else
      {
        c->args->insert(0, new arg("__lxsh_array_create") );
        params->need_array_create=true;
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
      params->need_array_set=true;
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
          c = make_cmd_varindex("__lxsh_map_get", varname, copy(index));
          params->need_map_get=true;
        }
        else
        {
          c = make_cmd_varindex("__lxsh_array_get", varname, copy(index));
          params->need_array_get=true;
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
        c->args->add(new arg("__lxsh_map_set") );
        params->need_map_set=true;
      }
      else
      {
        c->args->add(new arg("__lxsh_array_set") );
        params->need_array_set=true;
      }
      // __lxsh_array_set "$VAR"
      c->args->add( make_arg("\"$"+varname+"\"") );
      // __lxsh_array_set "$VAR" N
      c->args->add( index );
      // __lxsh_array_set "$VAR" N value
      c->args->add( value );
      // $(__lxsh_array_set "$VAR" N value)
      subshell_subarg* sb = new subshell_subarg(new subshell(c));

      it->second = new arg("=");
      it->second->add(sb);
      has_replaced=true;
    }
    else if(it->first != nullptr && it->second!=nullptr && it->second->first_sa_string().substr(0,3) == "+=(")
    {
      // array add: VAR+=()
      // can be done by creating a new array with old array + new
      params->need_array_create=true;

      std::string varname = it->first->varname;

      // extract arguments from =+(ARGS...)
      std::string gen=it->second->generate(0);
      gen=gen.substr(3);
      gen.pop_back();
      // create cmd out of arguments
      arglist* args = parse_arglist( gen.c_str(), gen.size(), 0 ).first;
      cmd* c = new cmd(args);
      // cmd first argument is __lxsh_array_create
      if(params->arrays[varname])
      {
        throw std::runtime_error("Cannot debashify VAR+=() on associative arrays");
      }
      else
      {
        c->args->insert(0, new arg("__lxsh_array_create") );
        params->need_array_create=true;
      }
      // second arg is varname
      c->args->insert(1, make_arg("\"$"+varname+"\"") );
      subshell_subarg* sb = new subshell_subarg(new subshell(c));
      // insert new value
      delete it->second;
      it->second = new arg("=");
      it->second->add(sb);
      has_replaced=true;

      // throw std::runtime_error("Cannot debashify VAR+=()");
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
  fifoN=${TMPDIR-/tmp}/lxshfifo_$(__lxsh_random_string 10)
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
      params->need_random_tmpfile = true;
      has_replaced=true;
      list* lst_insert = new list;
      std::string mkfifocmd="mkfifo";
      for(uint32_t i=0; i<affected_args.size(); i++)
      {
        // fifoN=${TMPDIR-/tmp}/lxshfifo_$(__lxsh_random_string 10)
        lst_insert->add( make_condlist( strf("__lxshfifo%u=$(__lxsh_random_tmpfile lxshfifo)", i) ) );
        mkfifocmd += strf(" \"$__lxshfifo%u\"", i);
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
        // {PSUB;} > "$__lxshfifoN"
        cbr->redirs.push_back( new redirect( affected_args[i].second ? "<" : ">", make_arg(strf("\"$__lxshfifo%u\"", i)) ) );
        // ( {PSUB;} > "$__lxshfifoN" )
        psub->lst->add( new condlist(cbr) );
        // ( {PSUB;} > "$__lxshfifoN" ; rm "$__lxshfifoN" )
        psub->lst->add( make_condlist(strf("rm \"$__lxshfifo%u\"", i)) );
        // ( {PSUB;} > "$__lxshfifoN" ; rm "$__lxshfifoN" ) &
        condlist* pscl = new condlist(psub);
        pscl->parallel=true;
        lst_insert->add( pscl );

        // replace the arg
        delete affected_args[i].first->sa[0];
        affected_args[i].first->sa[0] = new string_subarg("\"");
        affected_args[i].first->add( new variable_subarg( new variable(strf("__lxshfifo%u", i)) ) );
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

bool r_debashify(_obj* o, debashify_params* params)
{
  // global debashifies
  debashify_array_arithmetic(o, params);
  switch(o->type)
  {
    case _obj::_arg: {
      arg* t = dynamic_cast<arg*>(o);
      debashify_array_get(t, params);
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
//
void debashify(shmain* sh)
{
  debashify_params params;
  sh->shebang = "#!/bin/sh";
  recurse(r_debashify, sh, &params);
  if(params.need_random_string || params.need_random_tmpfile)
    sh->lst->insert(0, new condlist(make_block(RANDOM_TMPFILE_SH)));
  if(params.need_random_tmpfile)
    sh->lst->insert(0, new condlist(make_block(RANDOM_STRING_SH)));
  if(params.need_array_create)
    sh->lst->insert(0, new condlist(make_block(ARRAY_CREATE_SH)));
  if(params.need_array_set)
    sh->lst->insert(0, new condlist(make_block(ARRAY_SET_SH)));
  if(params.need_array_get)
    sh->lst->insert(0, new condlist(make_block(ARRAY_GET_SH)));
  if(params.need_map_create)
    sh->lst->insert(0, new condlist(make_block(MAP_CREATE_SH)));
  if(params.need_map_set)
    sh->lst->insert(0, new condlist(make_block(MAP_SET_SH)));
  if(params.need_map_get)
    sh->lst->insert(0, new condlist(make_block(MAP_GET_SH)));
}
