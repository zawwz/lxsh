#include "debashify.hpp"

#include "recursive.hpp"
#include "util.hpp"
#include "parse.hpp"
#include "struc_helper.hpp"

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

  if(args.size() == 3 && args[1]->string() == "==")
  {
    delete args[1]->sa[0];
    args[1]->sa[0] = new string_subarg("=");
  }

  if(args.size() == 3 && args[1]->string() == "=" && arg_has_char('*', args[2]))
  {
    delete args[1];
    args[1]=nullptr;
    case_block* tc = new case_block(args[0]);
    tc->cases.push_back( std::make_pair(std::vector<arg*>({args[2]}), make_list("true")) );
    tc->cases.push_back( std::make_pair(std::vector<arg*>({new arg("*")}), make_list("false")) );
    ret = tc;
  }
  else if(args.size() == 3 && args[1]->string() == "=~")
  {
    delete args[1];
    args[1]=nullptr;
    add_quotes(args[2]);
    ret = make_cmd( std::vector<arg*>({ new arg("expr"), args[0], new arg(":"), args[2] }) );
    ret->redirs.push_back(new redirect(">", new arg("/dev/null") ));
  }
  else // regular [ ]
  {
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

  if(in->firstarg_string() == "[[")
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

bool debashify_declare(cmd* in)
{
  if(in->firstarg_string() == "declare")
    throw std::runtime_error("Cannot debashify 'declare'");
  return false;
}

bool debashify_array_def(cmd* in)
{
  for(auto it: in->var_assigns)
  {
    if(it.second->size()>0 && it.second->sa[0]->type == _obj::subarg_string && it.second->sa[0]->generate(0) == "(")
      throw std::runtime_error("Cannot debashify 'VAR=()' arrays");
  }
  return false;
}

bool debashify_array_call(variable* in)
{
  if(in->index != nullptr)
    throw std::runtime_error("Cannot debashify 'VAR[I]' arrays");
  return false;
}

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
        cmd* printcmd = make_cmd("printf '%s\\n'");
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
  fifoN=${TMPDIR-/tmp}/lxshfifo_$(__lxsh_random 10)
  mkfifo "$fifoN"
  ( {PSUB;} [>|<] "$fifoN" ; rm "$fifoN") &
  CMD "$fifoN"
*/
bool debashify_procsub(list* lst)
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
      has_replaced=true;
      list* lst_insert = new list;
      std::string mkfifocmd="mkfifo";
      for(uint32_t i=0; i<affected_args.size(); i++)
      {
        // fifoN=${TMPDIR-/tmp}/lxshfifo_$(__lxsh_random 10)
        lst_insert->add( make_condlist( strf("__lxshfifo%u=${TMPDIR-/tmp}/lxshfifo_$(__lxsh_random 10)", i) ) );
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
        // deindex list
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

function* create_random_func()
{
  std::string code="{ tr -cd '[:alnum:]' </dev/urandom | head -c $1; }";
  function* fct=parse_function(code.c_str(), code.size(), 0).first;
  fct->name="__lxsh_random";
  return fct;
}

bool r_debashify(_obj* o, bool* need_random_func)
{
  switch(o->type)
  {
    case _obj::_variable: {
      variable* t = dynamic_cast<variable*>(o);
      debashify_array_call(t);
    } break;
    case _obj::_list: {
      list* t = dynamic_cast<list*>(o);
      if(debashify_procsub(t))
        *need_random_func = true;
    } break;
    case _obj::_pipeline: {
      pipeline* t = dynamic_cast<pipeline*>(o);
      debashify_herestring(t);
      debashify_bashtest(t);
    } break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(o);
      debashify_combined_redirects(t);
      debashify_declare(t);
      debashify_array_def(t);
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

void debashify(shmain* sh)
{
  bool need_random_func=false;
  sh->shebang = "#!/bin/sh";
  recurse(r_debashify, sh, &need_random_func);
  if(need_random_func)
    sh->lst->insert(0, new condlist(create_random_func()));
}
