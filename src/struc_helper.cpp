#include "struc_helper.hpp"

#include "parse.hpp"
#include "options.hpp"

// ** FUNCTIONS ** //

// makers

arg_t* make_arg(std::string const& in)
{
  return parse_arg(make_context(in)).first;
}

cmd_t* make_cmd(std::vector<const char*> const& args)
{
  cmd_t* ret = new cmd_t;
  ret->args = new arglist_t;
  for(auto it: args)
    ret->args->add(new arg_t(it));
  return ret;
}

cmd_t* make_cmd(std::vector<std::string> const& args)
{
  cmd_t* ret = new cmd_t;
  ret->args = new arglist_t;
  for(auto it: args)
    ret->args->add(new arg_t(it));
  return ret;
}

cmd_t* make_cmd(std::vector<arg_t*> const& args)
{
  cmd_t* ret = new cmd_t;
  ret->args = new arglist_t;
  for(auto it: args)
    ret->args->add(it);

  return ret;
}

cmd_t* make_cmd(std::string const& in)
{
  return parse_cmd(make_context(in)).first;
}

pipeline_t* make_pipeline(std::vector<block_t*> const& bls)
{
  pipeline_t* ret = new pipeline_t;
  for(auto it: bls)
    ret->add(it);

  return ret;
}

pipeline_t* make_pipeline(std::string const& in)
{
  return parse_pipeline(make_context(in)).first;
}

condlist_t* make_condlist(std::string const& in)
{
  return parse_condlist(make_context(in)).first;
}

list_t* make_list(std::string const& in)
{
  auto t = parse_list_until(make_context(in));
  return std::get<0>(t);
}

block_t* make_block(std::string const& in)
{
  return parse_block(make_context(in)).first;
}

cmd_t* make_printf(arg_t* in)
{
  cmd_t* prnt = make_cmd(std::vector<const char*>({"printf", "%s\\\\n"}));
  force_quotes(in);
  prnt->add(in);
  return prnt;
}

arithmetic_t* make_arithmetic(arg_t* a)
{
  if(a->sa.size() != 1)
  {
    cmd_t* prnt = make_printf(a);
    return new arithmetic_subshell_t(new subshell_t(prnt));
  }
  arithmetic_t* ret=nullptr;
  switch(a->sa[0]->type) {
    case _obj::subarg_string : {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(a->sa[0]);
      ret = new arithmetic_number_t(t->val);
    }; break;
    case _obj::subarg_variable : {
      subarg_variable_t* t = dynamic_cast<subarg_variable_t*>(a->sa[0]);
      ret = new arithmetic_variable_t(t->var);
      t->var = nullptr;
    }; break;
    case _obj::subarg_subshell : {
      subarg_subshell_t* t = dynamic_cast<subarg_subshell_t*>(a->sa[0]);
      ret = new arithmetic_subshell_t(t->sbsh);
      t->sbsh = nullptr;
    }; break;
    case _obj::subarg_arithmetic : {
      subarg_arithmetic_t* t = dynamic_cast<subarg_arithmetic_t*>(a->sa[0]);
      ret = t->arith;
      t->arith = nullptr;
    }; break;
    default: break;
  }
  delete a;
  return ret;
}

arithmetic_t* make_arithmetic(arg_t* arg1, std::string op, arg_t* arg2)
{
  return new arithmetic_operation_t(op, make_arithmetic(arg1), make_arithmetic(arg2));
}


// copy

arg_t* copy(arg_t* in) {
  std::string str = in->generate(0);
  return parse_arg(make_context(str)).first;
}

// modifiers

void force_quotes(arg_t* in)
{
  for(uint32_t i=0; i < in->sa.size() ; i++)
  {
    if(!in->sa[i]->quoted && (in->sa[i]->type == _obj::subarg_variable || in->sa[i]->type == _obj::subarg_subshell) )
    {
      in->sa[i]->quoted=true;
      in->insert(i+1, "\"");
      in->insert(i, "\"");
      i+=2;
    }
  }
}

void add_quotes(arg_t* in)
{
  for(uint32_t i=0; i < in->sa.size() ; i++)
    in->sa[i]->quoted=true;

  in->insert(0, new subarg_string_t("\""));
  in->add("\"");
}

// ** TESTERS ** //

bool arg_has_char(char c, arg_t* in)
{
  for(auto it: in->sa)
  {
    if(it->type == _obj::subarg_string)
    {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(it);
      if(t->val.find(c) != std::string::npos)
        return true;
    }
  }
  return false;
}

bool possibly_expands(arg_t* in)
{
  for(auto it: in->sa)
    if( (it->type == _obj::subarg_subshell || it->type == _obj::subarg_variable ) && it->quoted == false)
      return true;
  return false;
}

bool possibly_expands(arglist_t* in)
{
  for(auto it: in->args)
    if(possibly_expands(it))
      return true;
  return false;
}

// ** CLASS EXTENSIONS ** //

/// GETTERS ///

// property getters

bool cmd_t::has_var_assign()
{
  if(this->args == nullptr || this->args->size() == 0)
  {
    return this->var_assigns.size()>0;
  }
  return this->is_argvar();
}

size_t cmd_t::arglist_size()
{
  if(args==nullptr)
    return 0;
  else
    return args->size();
}

// string getters

bool arg_t::is_string()
{
  return sa.size() == 1 && sa[0]->type == _obj::subarg_string;
}

std::string arg_t::string()
{
  if(!this->is_string())
    return "";
  return dynamic_cast<subarg_string_t*>(sa[0])->val;
}

std::string arg_t::first_sa_string()
{
  if(sa.size() <=0 || sa[0]->type != _obj::subarg_string)
  return "";
  return dynamic_cast<subarg_string_t*>(sa[0])->val;
}

bool arg_t::can_expand()
{
  for(auto it: sa)
  {
    if(it->type != _obj::subarg_string && !it->quoted)
      return true;
  }
  return false;
}

bool arglist_t::can_expand()
{
  bool arg_expands=false;
  for(auto it: args)
  {
    arg_expands = it->can_expand();
    if(arg_expands)
      return true;
  }
  return false;
}

std::vector<std::string> arglist_t::strargs(uint32_t start)
{
  std::vector<std::string> ret;
  bool t=opt_minify;
  opt_minify=true;
  for(uint32_t i=start; i<args.size(); i++)
  {
    ret.push_back(args[i]->generate(0));
  }
  opt_minify=t;
  return ret;
}

std::string const& cmd_t::arg_string(uint32_t n)
{
  if(args!=nullptr && args->args.size()>n && args->args[n]->sa.size() == 1 && args->args[n]->sa[0]->type == _obj::subarg_string)
    return dynamic_cast<subarg_string_t*>(args->args[n]->sa[0])->val;
  return cmd_t::empty_string;
}

// subobject getters

block_t* condlist_t::first_block()
{
  if(pls.size() > 0 && pls[0]->cmds.size() > 0)
    return (pls[0]->cmds[0]);
  else
    return nullptr;
}

cmd_t* condlist_t::first_cmd()
{
  if(pls.size() > 0 && pls[0]->cmds.size() > 0 && pls[0]->cmds[0]->type == _obj::block_cmd)
    return dynamic_cast<cmd_t*>(pls[0]->cmds[0]);
  else
    return nullptr;
}

cmd_t* brace_t::single_cmd()
{
  if( lst->cls.size() == 1 && // only one condlist
      lst->cls[0]->pls.size() == 1 && // only one pipeline
      lst->cls[0]->pls[0]->cmds.size() == 1 && // only one block
      lst->cls[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd_t*>(lst->cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

cmd_t* subshell_t::single_cmd()
{
  if( lst->cls.size() == 1 && // only one condlist
      lst->cls[0]->pls.size() == 1 && // only one pipeline
      lst->cls[0]->pls[0]->cmds.size() == 1 && // only one block
      lst->cls[0]->pls[0]->cmds[0]->type == _obj::block_cmd) // block is a command
    return dynamic_cast<cmd_t*>(lst->cls[0]->pls[0]->cmds[0]); // return command
  return nullptr;
}

cmd_t* block_t::single_cmd()
{
  if(this->type == _obj::block_subshell)
    return dynamic_cast<subshell_t*>(this)->single_cmd();
  if(this->type == _obj::block_brace)
    return dynamic_cast<brace_t*>(this)->single_cmd();
  return nullptr;
}

/// MODIFIERS ///

// simple setters

void arg_t::set(std::string const& str)
{
  for(auto it: sa)
    delete it;
  sa.resize(0);
  sa.push_back(new subarg_string_t(str));
}

void condlist_t::prune_first_cmd()
{
  if(pls.size()>0 && pls[0]->cmds.size()>0)
  {
    delete pls[0]->cmds[0];
    pls[0]->cmds.erase(pls[0]->cmds.begin());
  }
}

// add/extend

void arg_t::insert(uint32_t i, std::string const& in)
{
  if(i>0 && i<=sa.size() && sa[i-1]->type == _obj::subarg_string)
  {
    subarg_string_t* t = dynamic_cast<subarg_string_t*>(sa[i-1]);
    t->val += in;
  }
  else if(i<sa.size() && sa[i]->type == _obj::subarg_string)
  {
    subarg_string_t* t = dynamic_cast<subarg_string_t*>(sa[i]);
    t->val = in + t->val;
  }
  else
    sa.insert(sa.begin()+i, new subarg_string_t(in));
}
void arg_t::add(std::string const& in)
{
  this->insert(this->size(), in);
}

void arg_t::insert(uint32_t i, subarg_t* val)
{
  if(val->type == _obj::subarg_string)
  {
    subarg_string_t* tval = dynamic_cast<subarg_string_t*>(val);
    if(i>0 && i<=sa.size() && sa[i-1]->type == _obj::subarg_string)
    {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(sa[i-1]);
      t->val += tval->val;
      delete val;
    }
    else if(i<sa.size() && sa[i]->type == _obj::subarg_string)
    {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(sa[i]);
      t->val = tval->val + t->val;
      delete val;
    }
    else
      sa.insert(sa.begin()+i, val);
  }
  else
    sa.insert(sa.begin()+i, val);
}
void arg_t::insert(uint32_t i, arg_t const& a)
{
  sa.insert(sa.begin()+i, a.sa.begin(), a.sa.end());
}

void arglist_t::insert(uint32_t i, arg_t* val)
{
  args.insert(args.begin()+i, val);
}
void arglist_t::insert(uint32_t i, arglist_t const& lst)
{
  args.insert(args.begin()+i, lst.args.begin(), lst.args.end());
}

void cmd_t::add(arg_t* in)
{
  if(args==nullptr)
    args = new arglist_t;

  args->add(in);
}

void condlist_t::add(pipeline_t* pl, bool or_op)
{
  if(pls.size() > 0)
    or_ops.push_back(or_op);
  pls.push_back(pl);
}

void list_t::insert(uint32_t i, condlist_t* val)
{
  if(i<0)
    cls.insert(cls.end(), val);
  else
    cls.insert(cls.begin()+i, val);
}
void list_t::insert(uint32_t i, list_t const& lst)
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

void condlist_t::negate()
{
  // invert commands
  for(uint32_t i=0; i<pls.size(); i++)
    pls[i]->negated = !pls[i]->negated;
  // invert bool operators
  for(uint32_t i=0; i<or_ops.size(); i++)
    or_ops[i] = !or_ops[i];
}
