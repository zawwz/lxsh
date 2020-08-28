#ifndef STRUC_HPP
#define STRUC_HPP

#include <string>
#include <vector>
#include <set>

#include <exception>
#include <stdexcept>

/*
structure:

block:
- group
  - brace:  condlist[]
  - subsh:  condlist[]
- cmd: arglist[]
- if
  - pair<condlist[]>[]

condlist:
  pipeline[]

pipeline:
  block[]

arglist:
  arg[]

arg:
- raw
- subarg[]

subarg:
- raw
- variable
- block: subshell


*/

#define AND_OP false
#define OR_OP  true

class condlist;
class block;
class pipeline;
class arg;
class subarg;

typedef std::vector<condlist> list_t;

block make_cmd(std::vector<std::string> args);

bool add_include(std::string const& file);

class arg
{
public:
  arg() { ; }
  arg(std::string const& str) {this->setstring(str);}

  void setstring(std::string const& str);

  std::string raw;

  std::vector<subarg> sa;

  std::string string();
  std::string generate(int ind);
};

class arglist
{
public:
  inline void add(arg const& in) { args.push_back(in); }
  inline void push_back(arg const& in) { args.push_back(in); }

  std::vector<arg> args;

  std::vector<std::string> strargs(uint32_t start);

  inline uint64_t size() { return args.size(); }
  inline arg& operator[](uint32_t i) { return args[i]; }

  std::string generate(int ind);
};

class redir
{
public:
  enum redirtype { none, write, append, read, raw } ;
  redir(redirtype in=none) { type=in; }
  redirtype type;
  arg val;
};

class block
{
public:
  enum blocktype { none, subshell, brace, main, cmd, function, case_block, for_block, if_block, while_block};
  block() { type=none; }
  block(blocktype in) { type=in; }
  blocktype type;
  // subshell/brace/main
  list_t cls;
  // cmd
  arglist args;

  // case
  arg carg;
  std::vector< std::pair<arg, list_t> > cases;

  // main: shebang
  // function: name
  std::string shebang;

  // subshell: return the containing cmd, if it is a single command
  block* single_cmd();

  std::string generate(int ind=0, bool print_shebang=true);

private:
  std::string generate_cmd(int ind);
  std::string generate_case(int ind);
};

class pipeline
{
public:
  pipeline() {;}
  pipeline(block const& bl) { cmds.push_back(bl); }
  inline void add(block bl) { this->cmds.push_back(bl); }
  std::vector<block> cmds;

  std::string generate(int ind);
};

class condlist
{
public:
  condlist() { parallel=false; }
  condlist(block const& pl) { parallel=false; this->add(pl);}
  condlist(pipeline const& pl) { parallel=false; this->add(pl);}
  void add(pipeline const& pl, bool or_op=false);

  bool parallel;
  // don't push_back here
  std::vector<bool> or_ops;
  std::vector<pipeline> pls;

  std::string generate(int ind);
};


class subarg
{
public:
  enum argtype { string, subshell };
  subarg(argtype in) { this->type=in; }
  subarg(std::string const& in="") { type=string; val=in; }
  subarg(block const& in) { type=subshell; sbsh=in; }

  argtype type;
  // raw string
  std::string val;
  // subshell
  block sbsh;

  std::string generate(int ind);
};


#endif //STRUC_HPP
