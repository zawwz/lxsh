#include "struc.hpp"

#include "util.hpp"

block make_cmd(std::vector<std::string> args)
{
  block cmd(block::cmd);
  for(auto it: args)
  {
    cmd.args.add(arg(it));
  }
  return cmd;
}

std::vector<std::string> arglist::strargs(uint32_t start)
{
  std::vector<std::string> ret;
  for(uint32_t i=start; i<args.size(); i++)
    ret.push_back(args[i].raw);
  return ret;
}

void arg::setstring(std::string const& str)
{
  sa.resize(0);
  sa.push_back(subarg(str));
}

void condlist::add(pipeline const& pl, bool or_op)
{
  if(this->pls.size() > 0)
    this->or_ops.push_back(or_op);
  this->pls.push_back(pl);
}

block* block::single_cmd()
{
  if(this->type == block::subshell)
  {
    if( cls.size() == 1 && // only one condlist
        cls[0].pls.size() == 1 && // only one pipeline
        cls[0].pls[0].cmds.size() == 1 && // only one block
        cls[0].pls[0].cmds[0].type == block::cmd) // block is a command
      return &(cls[0].pls[0].cmds[0]); // return command
  }
  return nullptr;
}

std::string arg::string()
{
  if(sa.size() > 1 || sa[0].type != subarg::string)
    return "";
  return sa[0].val;
}
