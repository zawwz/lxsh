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

class condlist_t;
class block_t;
class pipeline_t;
class arg_t;
class subarg_t;
class cmd_t;
class redirect_t;

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
  redirect_t* here_document=nullptr;
  char* here_delimitor=NULL;
};

struct generate_context {
  arg_t* here_document=nullptr;
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

// meta object type
class _obj
{
public:
  enum _objtype {
    subarg_string, subarg_variable, subarg_subshell, subarg_arithmetic, subarg_procsub,
    variable,
    redirect,
    arg,
    arglist,
    pipeline,
    condlist,
    list,
    arithmetic_operation, arithmetic_number, arithmetic_variable, arithmetic_parenthesis, arithmetic_subshell,
    block_subshell, block_brace, block_main, block_cmd, block_function, block_case, block_if, block_for, block_while };
  _objtype type;

  virtual ~_obj() {;}
  virtual std::string generate(int ind)=0;
};

// meta arithmetic type
class arithmetic_t : public _obj
{
public:
  virtual std::string generate(int ind)=0;
};

// meta subarg type
class subarg_t : public _obj
{
public:
  virtual ~subarg_t() {;}
  virtual std::string generate(int ind)=0;

  bool quoted;
};

class arg_t : public _obj
{
public:
  arg_t() { type=_obj::arg; forcequoted=false; }
  arg_t(std::string const& str, bool fquote=false) { type=_obj::arg; this->set(str); forcequoted=fquote; }
  arg_t(subarg_t* in, bool fquote=false) { type=_obj::arg; sa.push_back(in); forcequoted=fquote; }
  ~arg_t() { for(auto it: sa) delete it; }

  void set(std::string const& str);

  void insert(uint32_t i, subarg_t* val);
  void insert(uint32_t i, arg_t const& a);
  void insert(uint32_t i, std::string const& in);

  inline void add(subarg_t* in) { sa.push_back(in); }
  void add(std::string const& in);
  inline size_t size() { return sa.size(); }

  std::vector<subarg_t*> sa;

  // is forcequoted: var assign
  bool forcequoted;

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

class variable_t : public _obj
{
public:
  variable_t(std::string const& in="", arg_t* i=nullptr, bool def=false, bool ismanip=false, arg_t* m=nullptr) { type=_obj::variable; varname=in; index=i; definition=def; is_manip=ismanip; precedence=false; manip=m; }
  ~variable_t() {
    if(index!=nullptr) delete index;
    if(manip!=nullptr) delete manip;
  }

  std::string varname;
  bool definition;
  arg_t* index; // for bash specific

  bool is_manip;
  bool precedence;
  arg_t* manip;

  std::string generate(int ind);
};

// arglist

class arglist_t : public _obj
{
public:
  arglist_t() { type=_obj::arglist; }
  arglist_t(arg_t* in) { type=_obj::arglist; this->add(in); }
  ~arglist_t() { for( auto it: args ) delete it; }
  inline void add(arg_t* in) { args.push_back(in); }

  std::vector<arg_t*> args;

  std::vector<std::string> strargs(uint32_t start);

  // potentially expands into more arguments than its size
  bool can_expand();

  void insert(uint32_t i, arg_t* val);
  void insert(uint32_t i, arglist_t const& lst);

  inline size_t size() { return args.size(); }

  std::string generate(int ind);
};

class redirect_t : public _obj
{
public:
  redirect_t(std::string strop="") { type=_obj::redirect; op=strop; target=nullptr; here_document=nullptr; }
  redirect_t(arg_t* in) { type=_obj::redirect; target=in; here_document=nullptr; }
  redirect_t(std::string strop, arg_t* in) { type=_obj::redirect; op=strop; target=in; here_document=nullptr; }
  redirect_t(std::string strop, arg_t* in, arg_t* doc) { type=_obj::redirect; op=strop; target=in; here_document=doc; }
  ~redirect_t() {
    if(target != nullptr) delete target;
    if(here_document != nullptr) delete here_document;
  }

  std::string generate(int ind);

  std::string op;
  arg_t* target;
  arg_t* here_document;
};

// Meta block
class block_t : public _obj
{
public:
  block_t() { ; }
  virtual ~block_t() { for(auto it: redirs) delete it; }
  // cmd
  std::vector<redirect_t*> redirs;

  // subshell: return the containing cmd, if it is a single command
  cmd_t* single_cmd();

  std::string generate_redirs(int ind, std::string const& _str, generate_context* ctx);

  virtual std::string generate(int ind, generate_context* ctx)=0;
};

// PL

class pipeline_t : public _obj
{
public:
  pipeline_t(block_t* bl=nullptr) { type=_obj::pipeline; if(bl!=nullptr) cmds.push_back(bl); negated=false; }
  ~pipeline_t() { for(auto it: cmds) delete it; }
  inline void add(block_t* bl) { this->cmds.push_back(bl); }
  std::vector<block_t*> cmds;

  bool negated; // negated return value (! at start)

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); };
};

// CL

class condlist_t : public _obj
{
public:
  condlist_t() { type=_obj::condlist; parallel=false; }
  condlist_t(pipeline_t* pl) { type=_obj::condlist; parallel=false; this->add(pl); }
  condlist_t(block_t* bl);
  ~condlist_t() { for(auto it: pls) delete it; }

  bool parallel; // & at the end

  void add(pipeline_t* pl, bool or_op=false);

  // don't push_back here, use add() instead
  std::vector<pipeline_t*> pls;
  std::vector<bool> or_ops; // size of 1 less than pls, defines separator between pipelines

  void prune_first_cmd();

  block_t* first_block();
  cmd_t* first_cmd();
  cmd_t* get_cmd(std::string const& cmdname);

  void negate();

  std::string generate(int ind);
};

class list_t : public _obj
{
public:
  list_t() { type=_obj::list; }
  list_t(condlist_t* in) { type=_obj::list; this->add(in); }
  ~list_t() { for(auto it: cls) delete it; }

  void clear() { for(auto it: cls) delete it; cls.resize(0); }

  std::vector<condlist_t*> cls;
  inline void add(condlist_t* in) { cls.push_back(in); }

  condlist_t* last_cond() { return cls[cls.size()-1]; }

  void insert(uint32_t i, condlist_t* val);
  void insert(uint32_t i, list_t const& lst);

  size_t size() { return cls.size(); }

  std::string generate(int ind, bool first_indent);
  std::string generate(int ind) { return this->generate(ind, true); }
};

// block subtypes //

class cmd_t : public block_t
{
public:
  cmd_t(arglist_t* in=nullptr) { type=_obj::block_cmd; args=in; is_cmdvar=false; }
  ~cmd_t() {
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

  void add(arg_t* in);


  // preceding var assigns
  std::vector<std::pair<variable_t*,arg_t*>> var_assigns;

  // is a cmdvar type
  bool is_cmdvar;
  // var assigns on cmdvar
  std::vector<std::pair<variable_t*,arg_t*>> cmd_var_assigns;

  // check if cmd is this (ex: unset)
  bool is(std::string const& in);
  // for var assigns in special cmds (export, unset, read, local)
  bool is_argvar();
  std::vector<subarg_t*> subarg_vars();

  // returns true if command performs env var changes
  bool has_var_assign();

  arglist_t* args;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class shmain : public block_t
{
public:
  shmain(list_t* in=nullptr) { type=_obj::block_main; if(in == nullptr) lst = new list_t; else lst=in; }
  ~shmain() {
    if(lst!=nullptr) delete lst;
  }

  void concat(shmain* in);

  std::string filename;
  std::string shebang;
  list_t* lst;

  std::string generate(bool print_shebang=true, int ind=0);
  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class subshell_t : public block_t
{
public:
  subshell_t(list_t* in=nullptr) { type=_obj::block_subshell; lst=in; }
  subshell_t(block_t* in) { type=_obj::block_subshell; lst=new list_t(new condlist_t(in)); }
  ~subshell_t() {
    if(lst!=nullptr) delete lst;
  }

  cmd_t* single_cmd();

  list_t* lst;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class brace_t : public block_t
{
public:
  brace_t(list_t* in=nullptr) { type=_obj::block_brace; lst=in; }
  ~brace_t() {
    if(lst!=nullptr) delete lst;
  }

  cmd_t* single_cmd();

  list_t* lst;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class function_t : public block_t
{
public:
  function_t(list_t* in=nullptr) { type=_obj::block_function; lst=in; }
  ~function_t() {
    if(lst!=nullptr) delete lst;
  }

  std::string name;
  list_t* lst;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class case_t : public block_t
{
public:
  case_t(arg_t* in=nullptr) { type=_obj::block_case; carg=in; }
  ~case_t() {
    if(carg!=nullptr) delete carg;
    for( auto cit : cases )
    {
      for( auto ait : cit.first )
        delete ait;
      if(cit.second != nullptr) delete cit.second;
    }
  }

  arg_t* carg;
  std::vector< std::pair<std::vector<arg_t*>, list_t*> > cases;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class if_t : public block_t
{
public:
  if_t() { type=_obj::block_if; else_lst=nullptr; }
  ~if_t() {
    for(auto ifb: blocks)
    {
      if(ifb.first!=nullptr) delete ifb.first;
      if(ifb.second!=nullptr) delete ifb.second;
    }
    if(else_lst!=nullptr) delete else_lst;
  }

  std::vector< std::pair<list_t*,list_t*> > blocks;

  list_t* else_lst;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class for_t : public block_t
{
public:
  for_t(variable_t* in=nullptr, arglist_t* args=nullptr, list_t* lst=nullptr, bool ii=false) { type=_obj::block_for; var=in; iter=args; ops=lst; in_val=ii; }
  ~for_t() {
    if(iter!=nullptr) delete iter;
    if(ops!=nullptr) delete ops;
    if(var!=nullptr) delete var;
  }

  variable_t* var;

  arglist_t* iter;
  list_t* ops;

  bool in_val;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

class while_t : public block_t
{
public:
  while_t(list_t* a=nullptr, list_t* b=nullptr) { type=_obj::block_while; cond=a; ops=b; }
  ~while_t() {
    if(cond!=nullptr) delete cond;
    if(ops!=nullptr) delete ops;
  }

  condlist_t* real_condition() { return cond->last_cond(); }

  list_t* cond;
  list_t* ops;

  std::string generate(int ind, generate_context* ctx);
  std::string generate(int ind) { return this->generate(ind, nullptr); }
};

// Subarg subtypes //

class subarg_string_t : public subarg_t
{
public:
  subarg_string_t(std::string const& in="") { type=_obj::subarg_string; val=in; }
  ~subarg_string_t() {;}

  std::string val;

  std::string generate(int ind) { return val; }
};

class subarg_variable_t : public subarg_t
{
public:
  subarg_variable_t(variable_t* in=nullptr, bool inq=false) { type=_obj::subarg_variable; var=in; quoted=inq; }
  ~subarg_variable_t() {
    if(var!=nullptr) delete var;
  }

  variable_t* var;

  std::string generate(int ind) { return "$" + var->generate(ind); }
};

class subarg_arithmetic_t : public subarg_t
{
public:
  subarg_arithmetic_t(arithmetic_t* a=nullptr) { type=_obj::subarg_arithmetic; arith=a; }
  ~subarg_arithmetic_t() {
    if(arith!=nullptr) delete arith;
  }

  arithmetic_t* arith;

  std::string generate(int ind);
};

class subarg_subshell_t : public subarg_t
{
public:
  subarg_subshell_t(subshell_t* in=nullptr, bool inq=false, bool is_backtick=false) { type=_obj::subarg_subshell; sbsh=in; quoted=inq; backtick=is_backtick; }
  ~subarg_subshell_t() { if(sbsh != nullptr) delete sbsh; }

  subshell_t* sbsh;
  bool backtick;

  std::string generate(int ind);
};

class subarg_procsub_t : public subarg_t
{
public:
  subarg_procsub_t() { type=_obj::subarg_procsub; sbsh=nullptr; is_output=false; }
  subarg_procsub_t(bool output, subshell_t* in) { type=_obj::subarg_procsub; sbsh=in; is_output=output; }
  ~subarg_procsub_t() { if(sbsh!=nullptr) delete sbsh; }

  bool is_output;
  subshell_t* sbsh;

  std::string generate(int ind);
};

// Arithmetic subtypes //

class arithmetic_operation_t : public arithmetic_t
{
public:
  arithmetic_operation_t(std::string op="", arithmetic_t* a=nullptr, arithmetic_t* b=nullptr, bool pre=false) { type=_obj::arithmetic_operation; oper=op; val1=a; val2=b; precedence=pre; }
  ~arithmetic_operation_t() {
    if(val1 != nullptr) delete val1;
    if(val2 != nullptr) delete val2;
  }

  std::string oper;
  bool precedence;
  arithmetic_t *val1, *val2;
  std::string generate(int ind);
};

class arithmetic_subshell_t : public arithmetic_t
{
public:
  arithmetic_subshell_t(subshell_t* a=nullptr) { type=_obj::arithmetic_subshell; sbsh=a; }
  ~arithmetic_subshell_t() {
    if(sbsh!=nullptr) delete sbsh;
  }

  subshell_t* sbsh;

  std::string generate(int ind);
};

class arithmetic_parenthesis_t : public arithmetic_t
{
public:
  arithmetic_parenthesis_t(arithmetic_t* a=nullptr) { type=_obj::arithmetic_parenthesis; val=a; }
  ~arithmetic_parenthesis_t() {
    if(val!=nullptr) delete val;
  }

  arithmetic_t* val;

  std::string generate(int ind);
};

class arithmetic_number_t : public arithmetic_t
{
public:
  arithmetic_number_t(std::string const& a) { type=_obj::arithmetic_number; val=a; }

  std::string val;

  std::string generate(int ind) { return val; }
};

class arithmetic_variable_t : public arithmetic_t
{
public:
  arithmetic_variable_t(variable_t* in=nullptr) { type=_obj::arithmetic_variable; var=in; }
  ~arithmetic_variable_t() {
    if(var!=nullptr) delete var;
  }

  variable_t* var;

  std::string generate(int ind);
};

#endif //STRUC_HPP
