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
- subshell (command substitution)
- arithmetic
- variable
- procsub (bash specific process substitution)
  > NOTE: MUST be the only subarg in the arg

*/

// pre-definitions

#define AND_OP false
#define OR_OP  true

class condlist;
class block;
class pipeline;
class arg;
class subarg;
class cmd;
class redirect;

// structs

struct parse_context {
  const char* data=NULL;
  uint64_t size=0;
  uint64_t i=0;
  const char* filename="";
  bool bash=false;
  const char* expecting="";
  const char* here_delimiter="";
  const char* here_doc="";
  const char operator[](uint64_t a) { return data[a]; }
  bool has_errored=false;
  redirect* here_document=nullptr;
  char* here_delimitor=NULL;
};

struct generate_context {
  arg* here_document=nullptr;
};

// exceptions

class format_error : public std::exception
{
public:
  //! @brief Conctructor
  inline format_error(const std::string& what, const std::string& origin, const std::string& data, int where, std::string level="error")  { desc=what; index=where; filename=origin; sdat=data; severity=level; }
  inline format_error(const std::string& what, parse_context const& ctx, std::string level="error") { desc=what; index=ctx.i; filename=ctx.filename; sdat=ctx.data; severity=level; }
  //! @brief Error message
  inline const char * what () const throw () {return desc.c_str();}
  //! @brief Origin of the data, name of imported file, otherwise empty if generated
  inline const char * origin() const throw () {return filename.c_str();}
  //! @brief Data causing the exception
  inline const char * data() const throw () {return sdat.c_str();}
  //! @brief Severity of the exception
  inline const std::string level() const throw () {return severity.c_str();}
  //! @brief Where the error is located in the data
  inline const int where () const throw () {return index;}
private:
  std::string desc;
  int index;
  std::string filename;
  std::string sdat;
  std::string severity;
};

// objects

// type pack of condlist
typedef std::vector<arg*> arglist_t;

extern std::string g_origin;

// meta object type
class _obj
{
public:
  enum _objtype {
    subarg_string, subarg_variable, subarg_subshell, subarg_arithmetic, subarg_procsub,
    _variable,
    _redirect,
    _arg,
    _arglist,
    _pipeline,
    _condlist,
    _list,
    arithmetic_operation, arithmetic_number, arithmetic_variable, arithmetic_parenthesis, arithmetic_subshell,
    block_subshell, block_brace, block_main, block_cmd, block_function, block_case, block_if, block_for, block_while };
  _objtype type;

  virtual ~_obj() {;}
  virtual std::string generate(int ind)=0;
};

// meta arithmetic type
class arithmetic : public _obj
{
public:
  virtual std::string generate(int ind)=0;
};

// meta subarg type
class subarg : public _obj
{
public:
  virtual ~subarg() {;}
  virtual std::string generate(int ind)=0;

  bool quoted;
};

class arg : public _obj
{
public:
  arg() { type=_obj::_arg; }
  arg(std::string const& str) { type=_obj::_arg; this->set(str);}
  arg(subarg* in) { type=_obj::_arg; sa.push_back(in); }
  ~arg() { for(auto it: sa) delete it; }

  void set(std::string const& str);

  void insert(uint32_t i, subarg* val);
  void insert(uint32_t i, arg const& a);
  void insert(uint32_t i, std::string const& in);

  inline void add(subarg* in) { sa.push_back(in); }
  void add(std::string const& in);
  inline size_t size() { return sa.size(); }

  std::vector<subarg*> sa;

  bool is_string();
  // return if is a string and only one subarg
  std::string string();
  // return if the first subarg is a string
  std::string first_sa_string();

  // can expand into multiple arguments
  bool can_expand();

  inline bool equals(std::string const& in) { return this->string() == in; }

  std::string generate(int ind);
};

class variable : public _obj
{
public:
  variable(std::string const& in="", arg* i=nullptr, bool def=false, bool ismanip=false, arg* m=nullptr) { type=_obj::_variable; varname=in; index=i; definition=def; is_manip=ismanip; precedence=false; manip=m; }
  ~variable() {
    if(index!=nullptr) delete index;
    if(manip!=nullptr) delete manip;
  }

  std::string varname;
  bool definition;
  arg* index; // for bash specific

  bool is_manip;
  bool precedence;
  arg* manip;

  std::string generate(int ind);
};

// arglist

class arglist : public _obj
{
public:
  arglist() { type=_obj::_arglist; }
  arglist(arg* in) { type=_obj::_arglist; this->add(in); }
  ~arglist() { for( auto it: args ) delete it; }
  inline void add(arg* in) { args.push_back(in); }

  std::vector<arg*> args;

  std::vector<std::string> strargs(uint32_t start);

  // potentially expands into more arguments than its size
  bool can_expand();

  void insert(uint32_t i, arg* val);
  void insert(uint32_t i, arglist const& lst);

  inline size_t size() { return args.size(); }

  std::string generate(int ind);
};

class redirect : public _obj
{
public:
  redirect(std::string strop="") { type=_obj::_redirect; op=strop; target=nullptr; here_document=nullptr; }
  redirect(arg* in) { type=_obj::_redirect; target=in; here_document=nullptr; }
  redirect(std::string strop, arg* in) { type=_obj::_redirect; op=strop; target=in; here_document=nullptr; }
  redirect(std::string strop, arg* in, arg* doc) { type=_obj::_redirect; op=strop; target=in; here_document=doc; }
  ~redirect() {
    if(target != nullptr) delete target;
    if(here_document != nullptr) delete here_document;
  }

  std::string generate(int ind);

  std::string op;
  arg* target;
  arg* here_document;
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

  std::string generate_redirs(int ind, std::string const& _str, generate_context* ctx);

  virtual std::string generate(int ind, generate_context* ctx)=0;
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); };
};

// CL

class condlist : public _obj
{
public:
  condlist() { type=_obj::_condlist; parallel=false; }
  condlist(pipeline* pl) { type=_obj::_condlist; parallel=false; this->add(pl); }
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
  list(condlist* in) { type=_obj::_list; this->add(in); }
  ~list() { for(auto it: cls) delete it; }

  void clear() { for(auto it: cls) delete it; cls.resize(0); }

  std::vector<condlist*> cls;
  inline void add(condlist* in) { cls.push_back(in); }

  condlist* last_cond() { return cls[cls.size()-1]; }

  void insert(uint32_t i, condlist* val);
  void insert(uint32_t i, list const& lst);

  size_t size() { return cls.size(); }

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
  cmd(arglist* in=nullptr) { type=_obj::block_cmd; args=in; is_cmdvar=false; }
  ~cmd() {
    if(args!=nullptr) delete args;
    for(auto it: var_assigns) {
      delete it.first;
      delete it.second;
    }
    for(auto it: cmd_var_assigns) {
      delete it.first;
      delete it.second;
    }
  }

  static const std::string empty_string;

  std::string const& arg_string(uint32_t n);

  size_t arglist_size();

  void add(arg* in);


  // preceding var assigns
  std::vector<std::pair<variable*,arg*>> var_assigns;

  // is a cmdvar type
  bool is_cmdvar;
  // var assigns on cmdvar
  std::vector<std::pair<variable*,arg*>> cmd_var_assigns;

  // check if cmd is this (ex: unset)
  bool is(std::string const& in);
  // for var assigns in special cmds (export, unset, read, local)
  bool is_argvar();
  std::vector<subarg*> subarg_vars();

  arglist* args;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class shmain : public block
{
public:
  shmain(list* in=nullptr) { type=_obj::block_main; lst=in; }
  ~shmain() {
    if(lst!=nullptr) delete lst;
  }

  void concat(shmain* in);

  std::string filename;
  std::string shebang;
  list* lst;

  std::string generate(bool print_shebang=true, int ind=0);
  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class subshell : public block
{
public:
  subshell(list* in=nullptr) { type=_obj::block_subshell; lst=in; }
  subshell(block* in) { type=_obj::block_subshell; lst=new list(new condlist(in)); }
  ~subshell() {
    if(lst!=nullptr) delete lst;
  }

  cmd* single_cmd();

  list* lst;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class for_block : public block
{
public:
  for_block(variable* in=nullptr, arglist* args=nullptr, list* lst=nullptr) { type=_obj::block_for; var=in; iter=args; ops=lst; }
  ~for_block() {
    if(iter!=nullptr) delete iter;
    if(ops!=nullptr) delete ops;
    if(var!=nullptr) delete var;
  }

  variable* var;

  arglist* iter;
  list* ops;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
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
  variable_subarg(variable* in=nullptr, bool inq=false) { type=_obj::subarg_variable; var=in; quoted=inq; }
  ~variable_subarg() {
    if(var!=nullptr) delete var;
  }

  variable* var;

  std::string generate(int ind) { return "$" + var->generate(ind); }
};

class arithmetic_subarg : public subarg
{
public:
  arithmetic_subarg(arithmetic* a=nullptr) { type=_obj::subarg_arithmetic; arith=a; }
  ~arithmetic_subarg() {
    if(arith!=nullptr) delete arith;
  }

  arithmetic* arith;

  std::string generate(int ind);
};

class subshell_subarg : public subarg
{
public:
  subshell_subarg(subshell* in=nullptr, bool inq=false) { type=_obj::subarg_subshell; sbsh=in; quoted=inq; }
  ~subshell_subarg() { if(sbsh != nullptr) delete sbsh; }

  subshell* sbsh;

  std::string generate(int ind);
};

class procsub_subarg : public subarg
{
public:
  procsub_subarg() { type=_obj::subarg_procsub; sbsh=nullptr; is_output=false; }
  procsub_subarg(bool output, subshell* in) { type=_obj::subarg_procsub; sbsh=in; is_output=output; }
  ~procsub_subarg() { if(sbsh!=nullptr) delete sbsh; }

  bool is_output;
  subshell* sbsh;

  std::string generate(int ind);
};

// Arithmetic subtypes //

class operation_arithmetic : public arithmetic
{
public:
  operation_arithmetic(std::string op="", arithmetic* a=nullptr, arithmetic* b=nullptr, bool pre=false) { type=_obj::arithmetic_operation; oper=op; val1=a; val2=b; precedence=pre; }
  ~operation_arithmetic() {
    if(val1 != nullptr) delete val1;
    if(val2 != nullptr) delete val2;
  }

  std::string oper;
  bool precedence;
  arithmetic *val1, *val2;
  std::string generate(int ind);
};

class subshell_arithmetic : public arithmetic
{
public:
  subshell_arithmetic(subshell* a=nullptr) { type=_obj::arithmetic_subshell; sbsh=a; }
  ~subshell_arithmetic() {
    if(sbsh!=nullptr) delete sbsh;
  }

  subshell* sbsh;

  std::string generate(int ind);
};

class parenthesis_arithmetic : public arithmetic
{
public:
  parenthesis_arithmetic(arithmetic* a=nullptr) { type=_obj::arithmetic_parenthesis; val=a; }
  ~parenthesis_arithmetic() {
    if(val!=nullptr) delete val;
  }

  arithmetic* val;

  std::string generate(int ind);
};

class number_arithmetic : public arithmetic
{
public:
  number_arithmetic(std::string const& a) { type=_obj::arithmetic_number; val=a; }

  std::string val;

  std::string generate(int ind) { return val; }
};

class variable_arithmetic : public arithmetic
{
public:
  variable_arithmetic(variable* in=nullptr) { type=_obj::arithmetic_variable; var=in; }
  ~variable_arithmetic() {
    if(var!=nullptr) delete var;
  }

  variable* var;

  std::string generate(int ind);
};

#endif //STRUC_HPP
