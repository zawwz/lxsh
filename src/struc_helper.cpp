#include "struc.hpp"

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

condlist::condlist(pipeline* pl)
{
  type=_obj::_condlist;
  parallel=false;
  if(pl!=nullptr) this->add(pl);
}

condlist::condlist(block* bl)
{
  type=_obj::_condlist;
  parallel=false;
  this->add(new pipeline(bl));
}

void cmd::add_arg(arg* in)
{
  if(args==nullptr)
    args = new arglist;

  args->push_back(in);
}
