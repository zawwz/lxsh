#include "struc.hpp"

#include "util.hpp"
#include "options.hpp"

#include <unistd.h>

std::string g_origin="";

const std::string cmd::empty_string="";

std::vector<std::string> arglist::strargs(uint32_t start)
{
  std::vector<std::string> ret;
  bool t=opt_minimize;
  opt_minimize=true;
  for(uint32_t i=start; i<args.size(); i++)
  {
    ret.push_back(args[i]->generate(0));
  }
  opt_minimize=t;
  return ret;
}

void arg::setstring(std::string const& str)
{
  for(auto it: sa)
    delete it;
  sa.resize(0);
  sa.push_back(new string_subarg(str));
}

std::string arg::string()
{
  if(sa.size() != 1 || sa[0]->type != subarg::subarg_string)
    return "";
  return dynamic_cast<string_subarg*>(sa[0])->val;
}

void condlist::prune_first_cmd()
{
  if(pls.size()>0 && pls[0]->cmds.size()>0)
  {
    delete pls[0]->cmds[0];
    pls[0]->cmds.erase(pls[0]->cmds.begin());
  }
}

void condlist::add(pipeline* pl, bool or_op)
{
  if(this->pls.size() > 0)
    this->or_ops.push_back(or_op);
  this->pls.push_back(pl);
}

block* condlist::first_block()
{
  if(pls.size() > 0 && pls[0]->cmds.size() > 0)
    return (pls[0]->cmds[0]);
  else
    return nullptr;
}

cmd* condlist::first_cmd()
{
  if(pls.size() > 0 && pls[0]->cmds.size() > 0 && pls[0]->cmds[0]->type == _obj::block_cmd)
    return dynamic_cast<cmd*>(pls[0]->cmds[0]);
  else
    return nullptr;
}

cmd* block::single_cmd()
{
  if(this->type == _obj::block_subshell)
  {
    return dynamic_cast<subshell*>(this)->single_cmd();
  }
  if(this->type == _obj::block_brace)
  {
    return dynamic_cast<brace*>(this)->single_cmd();
  }
  return nullptr;
}

cmd* subshell::single_cmd()
{
  if( lst->size() == 1 && // only one condlist
      (*lst)[0]->pls.size() == 1 && // only one pipeline
      (*lst)[0]->pls[0]->cmds.size() == 1 && // only one block
      (*lst)[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd*>((*lst)[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

size_t cmd::arglist_size()
{
  if(args==nullptr)
    return 0;
  else
    return args->size();
}

cmd* brace::single_cmd()
{
  if( lst->size() == 1 && // only one condlist
      (*lst)[0]->pls.size() == 1 && // only one pipeline
      (*lst)[0]->pls[0]->cmds.size() == 1 && // only one block
      (*lst)[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd*>((*lst)[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}
cmd* condlist::get_cmd(std::string const& cmdname)
{
  for(auto pl: pls)
  {
    for(auto bl: pl->cmds)
    {
      if(bl->type == _obj::block_cmd)
      {
        cmd* c=dynamic_cast<cmd*>(bl);
        if(c->args->size()>0 && (*c->args)[0]->equals(cmdname) )
          return c;
      }
    }
  }
  return nullptr;
}

void shmain::concat(shmain* in)
{
  this->lst->cls.insert(this->lst->cls.end(), in->lst->cls.begin(), in->lst->cls.end());
  in->lst->cls.resize(0);
  if(this->shebang == "")
    this->shebang = in->shebang;
}

void condlist::negate()
{
  // invert commands
  for(uint32_t i=0; i<pls.size(); i++)
    pls[i]->negated = !pls[i]->negated;
  // invert bool operators
  for(uint32_t i=0; i<or_ops.size(); i++)
    or_ops[i] = !or_ops[i];
}

std::string const& cmd::firstarg_string()
{
  if(args!=nullptr && args->args.size()>0 && args->args[0]->sa.size() == 1 && args->args[0]->sa[0]->type == _obj::subarg_string)
    return dynamic_cast<string_subarg*>(args->args[0]->sa[0])->val;
  return cmd::empty_string;
}
