#ifndef STRUC_HPP
#define STRUC_HPP

#include <string>
#include <vector>

#include <exception>
#include <stdexcept>

/*
structure:

list_t : condlist[]
arglist_t : arg[]

block: can be one of
- main
    string  (shebang)
    list_t  (commands)
- brace
    list_t
- subshell
    list_t
- cmd: arglist
- case
    arg                (input)
    pair<arglist_t,list_t>[] (cases)
- if
    pair<list_t,list_t>[] (blocks)
    list_t                (else)
- for
    string    (variable name)
    arglist   (iterations)
    list_t    (execution)
- while
    list_t    (condition)
    list_t    (execution)


condlist:
  pipeline[]
  or_ops[]
    > always one smaller than pipeline
    > designates an OR if true, otherwise an AND

pipeline:
  block[]

arglist:
  arg[]

arg: has
  raw
  subarg[]    can have multiple subarguments if string and subshells

subarg: can be one of
- string
- block: subshell (substitution)
- arithmetic

*/

#define AND_OP false
#define OR_OP  true

class condlist;
class block;
class pipeline;
class arg;
class subarg;
class cmd;

// type pack of condlist
typedef std::vector<condlist*> list_t;
typedef std::vector<arg*> arglist_t;

cmd* make_cmd(std::vector<std::string> args);

bool add_include(std::string const& file);

// meta subarg type
class subarg
{
public:
  // type
  enum argtype { s_string, s_subshell, s_arithmetic };
  argtype type;

  virtual ~subarg() {;}

  std::string generate(int ind);
};

class arg
{
public:
  arg() { ; }
  arg(std::string const& str) {this->setstring(str);}
  ~arg() { for(auto it: sa) delete it; }

  void setstring(std::string const& str);

  // has to be set manually
  std::string raw;

  std::vector<subarg*> sa;

  // return if is a string and only one subarg
  std::string string();

  std::string generate(int ind);
};

// arglist

class arglist
{
public:
  ~arglist() { for( auto it: args ) delete it; }
  inline void add(arg* in) { args.push_back(in); }
  inline void push_back(arg* in) { args.push_back(in); }

  arglist_t args;

  std::vector<std::string> strargs(uint32_t start);

  inline uint64_t size() { return args.size(); }
  inline arg* operator[](uint32_t i) { return args[i]; }

  std::string generate(int ind);
};


// PL

class pipeline
{
public:
  pipeline() { negated=false; }
  pipeline(block* bl) { cmds.push_back(bl); negated=false; }
  inline void add(block* bl) { this->cmds.push_back(bl); }
  std::vector<block*> cmds;

  bool negated; // negated return value (! at start)

  std::string generate(int ind);
};

// CL

class condlist
{
public:
  condlist() { parallel=false; }
  condlist(pipeline const& pl) { parallel=false; this->add(new pipeline(pl));}
  condlist(pipeline* pl) { parallel=false; this->add(pl);}

  bool parallel; // & at the end

  void add(pipeline* pl, bool or_op=false);
  // don't push_back here, use add() instead
  std::vector<pipeline*> pls;
  std::vector<bool> or_ops; // size of 1 less than pls, defines separator between pipelines

  void negate();

  std::string generate(int ind, bool pre_indent=true);
};

// class redir
// {
// public:
//   enum redirtype { none, write, append, read, raw } ;
//   redir(redirtype in=none) { type=in; }
//   redirtype type;
//   arg val;
// };


class cmd;

// Meta block
class block
{
public:
  // type
  enum blocktype { block_subshell, block_brace, block_main, block_cmd, block_function, block_case, block_if, block_for, block_while, block_until };
  blocktype type;

  // ctor
  block() { redirs=nullptr; }
  virtual ~block() { if(redirs!=nullptr) delete redirs; }

  // cmd
  arglist* redirs;

  // subshell: return the containing cmd, if it is a single command
  cmd* single_cmd();

  std::string generate_redirs(int ind);

  virtual std::string generate(int ind)=0;
};

// block types

class subshell : public block
{
public:
  subshell() { type=block::block_subshell; }
  ~subshell() { for(auto it: cls) delete it; }

  cmd* single_cmd();

  list_t cls;

  std::string generate(int ind);
};
class brace : public block
{
public:
  brace() { type=block::block_brace; }
  ~brace() {
    if(redirs!=nullptr) delete redirs;
    for(auto it: cls) delete it; }

  cmd* single_cmd();

  list_t cls;

  std::string generate(int ind);
};

class shmain : public block
{
public:
  shmain() { type=block::block_main; }
  ~shmain() {
    if(redirs!=nullptr) delete redirs;
    for(auto it: cls) delete it; }

  std::string shebang;
  list_t cls;

  std::string generate(bool print_shebang=true, int ind=0);
  std::string generate(int ind);
};

class function : public block
{
public:
  function() { type=block::block_function; }
  ~function() {
    if(redirs!=nullptr) delete redirs;
    for(auto it: cls) delete it; }

  std::string name;
  list_t cls;

  std::string generate(int ind);
};

class cmd : public block
{
public:
  cmd(arglist* in=nullptr) { type=block::block_cmd; args=in; }
  ~cmd() {
    if(redirs!=nullptr) delete redirs;
    if(args!=nullptr) delete args; }

  static const std::string empty_string;

  std::string const& firstarg_raw();

  arglist* args;

  std::string generate(int ind);
};

class case_block : public block
{
public:
  case_block(arg* in=nullptr) { type=block::block_case; carg=in; }
  ~case_block() {
    if(redirs!=nullptr) delete redirs;
    if(carg!=nullptr) delete carg;
    for( auto cit : cases )
    {
      for( auto ait : cit.first )
        delete ait;
      for( auto lit : cit.second )
        delete lit;
    }
  }

  arg* carg;
  std::vector< std::pair<arglist_t, list_t> > cases;

  std::string generate(int ind);
};

class if_block : public block
{
public:
  if_block() { type=block::block_if; }
  ~if_block() {
    if(redirs!=nullptr) delete redirs;
    for(auto it: else_cls) delete it;
    for(auto ifb: blocks)
    {
      for(auto it: ifb.first)
        delete it;
      for(auto it: ifb.second)
        delete it;
    }
  }

  std::vector< std::pair<list_t,list_t> > blocks;

  list_t else_cls;

  std::string generate(int ind);
};

class for_block : public block
{
public:
  for_block(std::string const& name="", arglist* args=nullptr) { type=block::block_for; varname=name; iter=args; }
  ~for_block() {
    if(redirs!=nullptr) delete redirs;
    if(iter!=nullptr) delete iter;
    for(auto it: ops) delete it;
  }

  std::string varname;

  arglist* iter;
  list_t ops;

  std::string generate(int ind);
};

class while_block : public block
{
public:
  while_block() { type=block::block_while; }
  ~while_block() {
    if(redirs!=nullptr) delete redirs;
    for(auto it: cond) delete it;
    for(auto it: ops) delete it;
  }

  condlist* real_condition() { return *(cond.end()-1); }

  list_t cond;
  list_t ops;

  std::string generate(int ind);
};

// Subarg subtypes

class subarg_string : public subarg
{
public:
  subarg_string(std::string const& in="") { type=subarg::s_string; val=in; }

  std::string val;

  std::string generate(int ind) { return val; }
};

class subarg_arithmetic : public subarg
{
public:
  subarg_arithmetic() { type=subarg::s_arithmetic; }

  std::string val;

  std::string generate(int ind) { return "$(("+val+"))"; }
};

class subarg_subshell : public subarg
{
public:
  subarg_subshell(subshell* in=nullptr) { type=subarg::s_subshell; sbsh=in; }
  subarg_subshell(subshell in) { type=subarg::s_subshell; sbsh=new subshell(in); }
  ~subarg_subshell() { if(sbsh != nullptr) delete sbsh;}

  subshell* sbsh;

  std::string generate(int ind);
};


#endif //STRUC_HPP
