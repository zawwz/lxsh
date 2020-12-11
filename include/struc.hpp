#ifndef STRUC_HPP
#define STRUC_HPP

#include <string>
#include <vector>

#include <exception>
#include <stdexcept>

/*
structure:


block: can be one of
- main
    string  (shebang)
    list  (commands)
- brace
    list
- subshell
    list
- cmd: arglist
- case
    arg                       (input)
    pair<arg[],list>[]  (cases)
- if
    pair<list,list>[] (blocks)
    list                (else)
- for
    string    (variable name)
    arglist   (iterations)
    list    (execution)
- while
    list    (condition)
    list    (execution)

list:
  condlist[]

condlist:
  pipeline[]
  or_ops[]
    > always one smaller than pipeline
    > designates an OR if true, otherwise an AND

pipeline:
  block[]

arglist:
  arg[]

arg:
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
typedef std::vector<arg*> arglist_t;

extern std::string g_origin;

cmd* make_cmd(std::vector<std::string> args);

// meta object type
class _obj
{
public:
  enum _objtype {
    subarg_string, subarg_variable, subarg_subshell, subarg_arithmetic, subarg_manipulation,
    _redirect,
    _arg,
    _arglist,
    _pipeline,
    _condlist,
    _list,
    block_subshell, block_brace, block_main, block_cmd, block_function, block_case, block_if, block_for, block_while, block_until };
  _objtype type;

  virtual ~_obj() {;}
  virtual std::string generate(int ind)=0;
};

// meta subarg type
class subarg : public _obj
{
public:
  virtual ~subarg() {;}
  virtual std::string generate(int ind)=0;
};

class arg : public _obj
{
public:
  arg() { type=_obj::_arg; }
  arg(std::string const& str) { type=_obj::_arg; this->setstring(str);}
  ~arg() { for(auto it: sa) delete it; }

  void setstring(std::string const& str);

  std::vector<subarg*> sa;

  // return if is a string and only one subarg
  std::string string();

  inline bool equals(std::string const& in) { return this->string() == in; }

  std::string generate(int ind);
};

inline bool operator==(arg a, std::string const& b) { return a.equals(b); }

// arglist

class arglist : public _obj
{
public:
  arglist() { type=_obj::_arglist; }
  arglist(arg* in) { type=_obj::_arglist; this->add(in); }
  ~arglist() { for( auto it: args ) delete it; }
  inline void add(arg* in) { args.push_back(in); }
  inline void push_back(arg* in) { args.push_back(in); }

  std::vector<arg*> args;

  std::vector<std::string> strargs(uint32_t start);

  inline uint64_t size() { return args.size(); }
  inline arg* operator[](uint32_t i) { return args[i]; }

  std::string generate(int ind);
};

class redirect : public _obj
{
public:
  redirect(arg* in=nullptr) { type=_obj::_redirect; target=in; }
  ~redirect() { if(target != nullptr) delete target; }

  std::string generate(int ind);

  std::string op;
  arg* target;
};

// Meta block
class block : public _obj
{
public:
  block() { ; }
  virtual ~block() { for(auto it: redirs) delete it; }
  // cmd
  std::vector<redirect*> redirs;

  // subshell: return the containing cmd, if it is a single command
  cmd* single_cmd();

  std::string generate_redirs(int ind, std::string const& _str);

  virtual std::string generate(int ind)=0;
};

// PL

class pipeline : public _obj
{
public:
  pipeline(block* bl=nullptr) { type=_obj::_pipeline; if(bl!=nullptr) cmds.push_back(bl); negated=false; }
  ~pipeline() { for(auto it: cmds) delete it; }
  inline void add(block* bl) { this->cmds.push_back(bl); }
  std::vector<block*> cmds;

  bool negated; // negated return value (! at start)

  std::string generate(int ind);
};

// CL

class condlist : public _obj
{
public:
  condlist() { type=_obj::_condlist; parallel=false; }
  condlist(pipeline* pl);
  condlist(block* bl);
  ~condlist() { for(auto it: pls) delete it; }

  bool parallel; // & at the end

  void add(pipeline* pl, bool or_op=false);
  // don't push_back here, use add() instead
  std::vector<pipeline*> pls;
  std::vector<bool> or_ops; // size of 1 less than pls, defines separator between pipelines

  void prune_first_cmd();

  block* first_block();
  cmd* first_cmd();
  cmd* get_cmd(std::string const& cmdname);

  void negate();

  std::string generate(int ind);
};

class list : public _obj
{
public:
  list() { type=_obj::_list; }
  ~list() { for(auto it: cls) delete it; }

  std::vector<condlist*> cls;

  condlist* last_cond() { return cls[cls.size()-1]; }

  size_t size() { return cls.size(); }
  condlist* operator[](uint32_t i) { return cls[i]; }

  std::string generate(int ind, bool first_indent);
  std::string generate(int ind) { return this->generate(ind, true); }
};

// class redir
// {
// public:
//   enum redirtype { none, write, append, read, raw } ;
//   redir(redirtype in=none) { type=in; }
//   redirtype type;
//   arg val;
// };

// block subtypes //

class cmd : public block
{
public:
  cmd(arglist* in=nullptr) { type=_obj::block_cmd; args=in; }
  ~cmd() {
    if(args!=nullptr) delete args;
    for(auto it: var_assigns) delete it.second;
  }

  static const std::string empty_string;

  std::string const& firstarg_string();

  size_t arglist_size();

  void add_arg(arg* in);


  // preceding var assigns
  std::vector<std::pair<std::string,arg*>> var_assigns;

  // check if cmd is this (ex: unset)
  bool is(std::string const& in);
  // for var assigns in special cmds (export, unset, read, local)
  bool is_argvar();
  std::vector<subarg*> subarg_vars();

  arglist* args;

  std::string generate(int ind);
};

class shmain : public block
{
public:
  shmain(list* in=nullptr) { type=_obj::block_main; lst=in; }
  ~shmain() {
    if(lst!=nullptr) delete lst;
  }

  bool is_dev_file() { return filename.substr(0,5) == "/dev/"; }

  void concat(shmain* in);

  std::string filename;
  std::string shebang;
  list* lst;

  std::string generate(bool print_shebang=true, int ind=0);
  std::string generate(int ind);
};

class subshell : public block
{
public:
  subshell(list* in=nullptr) { type=_obj::block_subshell; lst=in; }
  ~subshell() {
    if(lst!=nullptr) delete lst;
  }

  cmd* single_cmd();

  list* lst;

  std::string generate(int ind);
};

class brace : public block
{
public:
  brace(list* in=nullptr) { type=_obj::block_brace; lst=in; }
  ~brace() {
    if(lst!=nullptr) delete lst;
  }

  cmd* single_cmd();

  list* lst;

  std::string generate(int ind);
};

class function : public block
{
public:
  function(list* in=nullptr) { type=_obj::block_function; lst=in; }
  ~function() {
    if(lst!=nullptr) delete lst;
  }

  std::string name;
  list* lst;

  std::string generate(int ind);
};

class case_block : public block
{
public:
  case_block(arg* in=nullptr) { type=_obj::block_case; carg=in; }
  ~case_block() {
    if(carg!=nullptr) delete carg;
    for( auto cit : cases )
    {
      for( auto ait : cit.first )
        delete ait;
      if(cit.second != nullptr) delete cit.second;
    }
  }

  arg* carg;
  std::vector< std::pair<std::vector<arg*>, list*> > cases;

  std::string generate(int ind);
};

class if_block : public block
{
public:
  if_block() { type=_obj::block_if; else_lst=nullptr; }
  ~if_block() {
    for(auto ifb: blocks)
    {
      if(ifb.first!=nullptr) delete ifb.first;
      if(ifb.second!=nullptr) delete ifb.second;
    }
    if(else_lst!=nullptr) delete else_lst;
  }

  std::vector< std::pair<list*,list*> > blocks;

  list* else_lst;

  std::string generate(int ind);
};

class for_block : public block
{
public:
  for_block(std::string const& name="", arglist* args=nullptr, list* lst=nullptr) { type=_obj::block_for; varname=name; iter=args; ops=lst; }
  ~for_block() {
    if(iter!=nullptr) delete iter;
    if(ops!=nullptr) delete ops;
  }

  std::string varname;

  arglist* iter;
  list* ops;

  std::string generate(int ind);
};

class while_block : public block
{
public:
  while_block(list* a=nullptr, list* b=nullptr) { type=_obj::block_while; cond=a; ops=b; }
  ~while_block() {
    if(cond!=nullptr) delete cond;
    if(ops!=nullptr) delete ops;
  }

  condlist* real_condition() { return cond->last_cond(); }

  list* cond;
  list* ops;

  std::string generate(int ind);
};

// Subarg subtypes //

class string_subarg : public subarg
{
public:
  string_subarg(std::string const& in="") { type=_obj::subarg_string; val=in; }
  ~string_subarg() {;}

  std::string val;

  std::string generate(int ind) { return val; }
};

class variable_subarg : public subarg
{
public:
  variable_subarg(std::string const& in="") { type=_obj::subarg_variable; varname=in; }
  ~variable_subarg() {;}

  std::string varname;

  std::string generate(int ind) { return "$" + varname; }
};

class arithmetic_subarg : public subarg
{
public:
  arithmetic_subarg() { type=_obj::subarg_arithmetic; }
  ~arithmetic_subarg() {;}

  std::string val;

  std::string generate(int ind) { return "$(("+val+"))"; }
};

class subshell_subarg : public subarg
{
public:
  subshell_subarg(subshell* in=nullptr, bool inq=false) { type=_obj::subarg_subshell; sbsh=in; quoted=inq; }
  ~subshell_subarg() { if(sbsh != nullptr) delete sbsh; }

  subshell* sbsh;
  bool quoted;

  std::string generate(int ind);
};

class manipulation_subarg : public subarg
{
public:
  manipulation_subarg(arg* in=nullptr) { type=_obj::subarg_manipulation; size=false; manip=in; }
  ~manipulation_subarg() { if(manip!=nullptr) delete manip; }

  bool size;
  std::string varname;
  arg* manip;

  std::string generate(int ind);
};


#endif //STRUC_HPP
