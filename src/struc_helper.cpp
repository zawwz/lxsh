#include "struc_helper.hpp"

#include "parse.hpp"
#include "options.hpp"

// ** FUNCTIONS ** //

// makers

arg* make_arg(std::string const& in)
{
  return parse_arg(in.c_str(), in.size(), 0).first;
}

cmd* make_cmd(std::vector<std::string> const& args)
{
  cmd* ret = new cmd;
  ret->args = new arglist;
  for(auto it: args)
    ret->args->add(new arg(it));

  return ret;
}

cmd* make_cmd(std::vector<arg*> const& args)
{
  cmd* ret = new cmd;
  ret->args = new arglist;
  for(auto it: args)
    ret->args->add(it);

  return ret;
}

cmd* make_cmd(std::string const& in)
{
  return parse_cmd(in.c_str(), in.size(), 0).first;
}

pipeline* make_pipeline(std::vector<block*> const& bls)
{
  pipeline* ret = new pipeline;
  for(auto it: bls)
    ret->add(it);

  return ret;
}

pipeline* make_pipeline(std::string const& in)
{
  return parse_pipeline(in.c_str(), in.size(), 0).first;
}

condlist* make_condlist(std::string const& in)
{
  return parse_condlist(in.c_str(), in.size(), 0).first;
}

list* make_list(std::string const& in)
{
  return parse_list_until(in.c_str(), in.size(), 0, 0).first;
}

// modifiers

void force_quotes(arg* in)
{
  for(uint32_t i=0; i < in->sa.size() ; i++)
  {
    if(!in->sa[i]->quoted && (in->sa[i]->type == _obj::subarg_variable || in->sa[i]->type == _obj::subarg_manipulation || in->sa[i]->type == _obj::subarg_subshell) )
    {
      in->sa[i]->quoted=true;
      in->insert(i+1, new string_subarg("\""));
      in->insert(i, new string_subarg("\""));
      i+=2;
    }
  }
}

void add_quotes(arg* in)
{
  for(uint32_t i=0; i < in->sa.size() ; i++)
    in->sa[i]->quoted=true;

  in->insert(0, new string_subarg("\""));
  in->add(new string_subarg("\""));
}

// ** TESTERS ** //

bool arg_has_char(char c, arg* in)
{
  for(auto it: in->sa)
  {
    if(it->type == _obj::subarg_string)
    {
      string_subarg* t = dynamic_cast<string_subarg*>(it);
      if(t->val.find(c) != std::string::npos)
        return true;
    }
  }
  return false;
}

// ** CLASS EXTENSIONS ** //

/// GETTERS ///

// property getters

size_t cmd::arglist_size()
{
  if(args==nullptr)
    return 0;
  else
    return args->size();
}

// string getters

std::string arg::string()
{
  if(sa.size() != 1 || sa[0]->type != subarg::subarg_string)
    return "";
  return dynamic_cast<string_subarg*>(sa[0])->val;
}

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

std::string const& cmd::firstarg_string()
{
  if(args!=nullptr && args->args.size()>0 && args->args[0]->sa.size() == 1 && args->args[0]->sa[0]->type == _obj::subarg_string)
    return dynamic_cast<string_subarg*>(args->args[0]->sa[0])->val;
  return cmd::empty_string;
}

// subobject getters

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

cmd* brace::single_cmd()
{
  if( lst->cls.size() == 1 && // only one condlist
      lst->cls[0]->pls.size() == 1 && // only one pipeline
      lst->cls[0]->pls[0]->cmds.size() == 1 && // only one block
      lst->cls[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd*>(lst->cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

cmd* subshell::single_cmd()
{
  if( lst->cls.size() == 1 && // only one condlist
      lst->cls[0]->pls.size() == 1 && // only one pipeline
      lst->cls[0]->pls[0]->cmds.size() == 1 && // only one block
      lst->cls[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd*>(lst->cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

cmd* block::single_cmd()
{
  if(this->type == _obj::block_subshell)
    return dynamic_cast<subshell*>(this)->single_cmd();
  if(this->type == _obj::block_brace)
    return dynamic_cast<brace*>(this)->single_cmd();
  return nullptr;
}

/// MODIFIERS ///

// simple setters

void arg::set(std::string const& str)
{
  for(auto it: sa)
    delete it;
  sa.resize(0);
  sa.push_back(new string_subarg(str));
}

void condlist::prune_first_cmd()
{
  if(pls.size()>0 && pls[0]->cmds.size()>0)
  {
    delete pls[0]->cmds[0];
    pls[0]->cmds.erase(pls[0]->cmds.begin());
  }
}

// add/extend

void arg::insert(uint32_t i, std::string const& in)
{
  this->insert(i, new string_subarg(in));
}
void arg::add(std::string const& in)
{
  this->add(new string_subarg(in));
}

void arg::insert(uint32_t i, subarg* val)
{
  sa.insert(sa.begin()+i, val);
}
void arg::insert(uint32_t i, arg const& a)
{
  sa.insert(sa.begin()+i, a.sa.begin(), a.sa.end());
}

void arglist::insert(uint32_t i, arg* val)
{
  args.insert(args.begin()+i, val);
}
void arglist::insert(uint32_t i, arglist const& lst)
{
  args.insert(args.begin()+i, lst.args.begin(), lst.args.end());
}

void cmd::add(arg* in)
{
  if(args==nullptr)
    args = new arglist;

  args->add(in);
}

void condlist::add(pipeline* pl, bool or_op)
{
  if(pls.size() > 0)
    or_ops.push_back(or_op);
  pls.push_back(pl);
}

void list::insert(uint32_t i, condlist* val)
{
  if(i<0)
    cls.insert(cls.end(), val);
  else
    cls.insert(cls.begin()+i, val);
}
void list::insert(uint32_t i, list const& lst)
{
  if(i<0)
    cls.insert(cls.end(), lst.cls.begin(), lst.cls.end());
  else
    cls.insert(cls.begin()+i, lst.cls.begin(), lst.cls.end());
}

void shmain::concat(shmain* in)
{
  lst->insert(lst->size(), *in->lst);
  in->lst->cls.resize(0);
  if(this->shebang == "")
    this->shebang = in->shebang;
}

// special modifiers

void condlist::negate()
{
  // invert commands
  for(uint32_t i=0; i<pls.size(); i++)
    pls[i]->negated = !pls[i]->negated;
  // invert bool operators
  for(uint32_t i=0; i<or_ops.size(); i++)
    or_ops[i] = !or_ops[i];
}
