#include "struc.hpp"

#include "util.hpp"

#include <unistd.h>

const std::string cmd::empty_string="";

cmd* make_cmd(std::vector<std::string> args)
{
  cmd* ret = new cmd();
  ret->args = new arglist();
  for(auto it: args)
  {
    ret->args->add(new arg(it));
  }
  return ret;
}

std::vector<std::string> arglist::strargs(uint32_t start)
{
  std::vector<std::string> ret;
  for(uint32_t i=start; i<args.size(); i++)
    ret.push_back(args[i]->raw);
  return ret;
}

void arg::setstring(std::string const& str)
{
  for(auto it: sa)
    delete it;
  sa.resize(0);
  sa.push_back(new subarg_string(str));
  raw = str;
}

std::string arg::string()
{
  if(sa.size() > 1 || sa[0]->type != subarg::s_string)
    return "";
  return sa[0]->generate(0);
}

void condlist::add(pipeline* pl, bool or_op)
{
  if(this->pls.size() > 0)
    this->or_ops.push_back(or_op);
  this->pls.push_back(pl);
}

cmd* block::single_cmd()
{
  if(this->type == block::block_subshell)
  {
    return dynamic_cast<subshell*>(this)->single_cmd();
  }
  if(this->type == block::block_brace)
  {
    return dynamic_cast<brace*>(this)->single_cmd();
  }
  return nullptr;
}

cmd* subshell::single_cmd()
{
  if( cls.size() == 1 && // only one condlist
      cls[0]->pls.size() == 1 && // only one pipeline
      cls[0]->pls[0]->cmds.size() == 1 && // only one block
      cls[0]->pls[0]->cmds[0]->type == block::block_cmd) // block is a command
    return dynamic_cast<cmd*>(cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

cmd* brace::single_cmd()
{
  if( cls.size() == 1 && // only one condlist
      cls[0]->pls.size() == 1 && // only one pipeline
      cls[0]->pls[0]->cmds.size() == 1 && // only one block
      cls[0]->pls[0]->cmds[0]->type == block::block_cmd) // block is a command
    return dynamic_cast<cmd*>(cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
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

std::string const& cmd::firstarg_raw()
{
  if(args!=nullptr && args->size()>0)
    return args->args[0]->raw;
  return cmd::empty_string;
}
