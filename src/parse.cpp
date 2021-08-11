#include "parse.hpp"

#include <fstream>
#include <strings.h>
#include <string.h>

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"

#define ORIGIN_NONE ""

// macro

// constants
const std::set<std::string> posix_cmdvar = { "export", "unset", "local", "read", "getopts" };
const std::set<std::string> bash_cmdvar  = { "readonly", "declare", "typeset" };

const std::set<std::string> arithmetic_precedence_operators = { "!", "~", "+", "-" };
const std::set<std::string> arithmetic_operators = { "+", "-", "*", "/", "+=", "-=", "*=", "/=", "=", "==", "!=", "&", "|", "^", "<<", ">>", "&&", "||" };

const std::set<std::string> all_reserved_words = { "if", "then", "else", "fi", "case", "esac", "for", "while", "do", "done", "{", "}" };
const std::set<std::string> out_reserved_words = { "then", "else", "fi", "esac", "do", "done", "}" };

// stuff

std::string unexpected_token(char c)
{
  std::string print;
  print += c;
  if(c == '\n')
    print="\\n";
  return "Unexpected token '"+print+"'";
}

std::string unexpected_token(std::string const& s)
{
  return "Unexpected token '"+s+"'";
}

void parse_error(std::string const& message, parse_context& ctx)
{
  printFormatError(format_error(message, ctx));
  ctx.has_errored=true;
}

void parse_error(std::string const& message, parse_context& ctx, uint64_t i)
{
  parse_context newctx = ctx;
  newctx.i = i;
  printFormatError(format_error(message, newctx));
  ctx.has_errored=true;
}

std::string expecting(std::string const& word)
{
  if(word != "")
    return ", expecting '"+word+"'";
  else
    return "";
}

std::string expecting(const char* word)
{
  if(word == NULL)
    return expecting(std::string());
  else
    return expecting(std::string(word));
}

// basic char utils

bool has_common_char(const char* str1, const char* str2)
{
  uint32_t i=0;
  while(str1[i]!=0)
  {
    if(is_in(str1[i], str2))
      return true;
  }
  return false;
}

bool valid_name(std::string const& str)
{
  if(!is_alpha(str[0]) && str[0] != '_') return false;
  for(auto it: str)
  {
    if(! (is_alphanum(it) || it=='_' ) )
      return false;
  }
  return true;
}

// string utils

parse_context make_context(std::string const& in, std::string const& filename, bool bash)
{
  parse_context ctx = { .data=in.c_str(), .size=in.size(), .filename=filename.c_str(), .bash=bash};
  return ctx;
}

parse_context make_context(parse_context ctx, std::string const& in, std::string const& filename, bool bash)
{
  if(in != "")
  {
    ctx.data = in.c_str();
    ctx.size = in.size();
  }
  if(filename != "")
    ctx.filename = filename.c_str();
  if(bash)
    ctx.bash = bash;
  ctx.i=0;

  return ctx;
}

parse_context make_context(parse_context ctx, uint64_t i)
{
  ctx.i = i;
  return ctx;
}

parse_context operator+(parse_context ctx, int64_t a)
{
  ctx.i += a;
  return ctx;
}

parse_context operator-(parse_context ctx, int64_t a)
{
  ctx.i -= a;
  return ctx;
}

uint32_t skip_chars(const char* in, uint32_t size, uint32_t start, const char* set)
{
  for(uint32_t i=start; i<size ; i++)
  {
    if(!is_in(in[i],set))
      return i;
  }
  return size;
}

uint32_t skip_until(const char* in, uint32_t size, uint32_t start, const char* set)
{
  for(uint32_t i=start; i<size ; i++)
  {
    if(is_in(in[i],set))
      return i;
  }
  return size;
}

uint32_t skip_unread(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  while(true)
  {
    i = skip_chars(in, size, i, SEPARATORS);
    if(in[i] != '#') // not a comment
      return i;
    i = skip_until(in, size, i, "\n"); //skip to endline
  }
}

uint32_t skip_unread_noline(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  while(true)
  {
    i = skip_chars(in, size, i, SPACES);
    if(in[i] != '#') // not a comment
      return i;
    i = skip_until(in, size, i, "\n"); //skip to endline
  }
}

uint32_t word_eq(const char* word, const char* in, uint32_t size, uint32_t start, const char* end_set)
{
  uint32_t i=start;
  uint32_t wordsize=strlen(word);
  if(wordsize > size-i)
    return false;
  if(strncmp(word, in+i, wordsize) == 0)
  {
    if(end_set==NULL)
      return true;
    // end set
    if(wordsize < size-i)
      return is_in(in[i+wordsize], end_set);
  }
  return false;
}

std::pair<std::string,uint32_t> get_word(parse_context ctx, const char* end_set)
{
  uint32_t start=ctx.i;
  while(ctx.i<ctx.size && !is_in(ctx[ctx.i], end_set))
    ctx.i++;

  return std::make_pair(std::string(ctx.data+start, ctx.i-start), ctx.i);
}

// parse fcts

std::pair<variable*, parse_context> parse_var(parse_context ctx, bool specialvars, bool array)
{
  variable* ret=nullptr;
  std::string varname;
  uint32_t start=ctx.i;

  // special vars
  if(specialvars && (is_in(ctx[ctx.i], SPECIAL_VARS) || (ctx[ctx.i]>='0' && ctx[ctx.i]<='9')) )
  {
    varname=ctx[ctx.i];
    ctx.i++;
  }
  else // varname
  {
    while(ctx.i<ctx.size && (is_alphanum(ctx[ctx.i]) || ctx[ctx.i] == '_') )
      ctx.i++;
    varname = std::string(ctx.data+start, ctx.i-start);
  }
  if(varname != "")
  {
    ret = new variable(varname);
    if(ctx.bash && array && ctx[ctx.i]=='[')
    {
      ctx.i++;
      auto pp=parse_arg(ctx, ARRAY_ARG_END);
      ret->index=pp.first;
      ctx = pp.second;
      if(ctx[ctx.i] != ']')
      {
        parse_error( "Expecting ']'", ctx );
        return std::make_pair(ret, ctx);
      }
      ctx.i++;
    }
  }

  return std::make_pair(ret, ctx);
}

std::pair<std::string, uint32_t> get_operator(parse_context ctx)
{
  std::string ret;
  uint32_t start=ctx.i;

  while(!is_alphanum(ctx[ctx.i]) && !is_in(ctx[ctx.i], ARITHMETIC_OPERATOR_END))
    ctx.i++;

  ret = std::string(ctx.data+start, ctx.i-start);

  return std::make_pair(ret, ctx.i);
}

//** HERE **//

// parse an arithmetic
// ends at ))
// temporary, to improve
std::pair<arithmetic*, parse_context> parse_arithmetic(parse_context ctx)
{
  arithmetic* ret = nullptr;

  ctx.i = skip_chars(ctx, SEPARATORS);
  if(ctx.i>ctx.size || ctx[ctx.i] == ')')
  {
    parse_error( "Unexpected end of arithmetic", ctx );
    return std::make_pair(ret, ctx);
  }

  auto po = get_operator(ctx);
  if(is_in_set(po.first, arithmetic_precedence_operators))
  {
    ctx.i = po.second;
    auto pa = parse_arithmetic(ctx);
    ret = new operation_arithmetic(po.first, pa.first, nullptr, true);
    ctx=pa.second;
  }
  else
  {
    variable_arithmetic* ttvar=nullptr; // for categorizing definitions
    if(ctx[ctx.i]=='-' || is_num(ctx[ctx.i]))
    {
      uint32_t j=ctx.i;
      if(ctx[ctx.i]=='-')
        ctx.i++;
      while(is_num(ctx[ctx.i]))
        ctx.i++;
      ret = new number_arithmetic( std::string(ctx.data+j, ctx.i-j) );
    }
    else if(word_eq("$(", ctx))
    {
      ctx.i+=2;
      auto ps = parse_subshell(ctx);
      ret = new subshell_arithmetic(ps.first);
      ctx=ps.second;
    }
    else if(word_eq("${", ctx))
    {
      ctx.i+=2;
      auto pm = parse_manipulation(ctx);
      ret = new variable_arithmetic(pm.first);
      ctx=pm.second;
    }
    else if(ctx[ctx.i] == '(')
    {
      ctx.i++;
      auto pa = parse_arithmetic(ctx);
      ret = pa.first;
      ctx = pa.second;
      ctx.i++;
    }
    else
    {
      bool specialvars=false;
      if(ctx[ctx.i] == '$')
      {
        specialvars=true;
        ctx.i++;
      }
      auto pp = parse_var(ctx, specialvars, true);
      ttvar = new variable_arithmetic(pp.first);
      ret = ttvar;
      ctx=pp.second;
    }

    ctx.i = skip_chars(ctx, SEPARATORS);
    auto po = get_operator(ctx);
    if(po.first != "")
    {
      if(!is_in_set(po.first, arithmetic_operators))
      {
        parse_error( "Unknown arithmetic operator: "+po.first, ctx);
      }
      arithmetic* val1 = ret;
      ctx.i=po.second;
      auto pa = parse_arithmetic(ctx);
      arithmetic* val2 = pa.first;
      ctx = pa.second;
      ret = new operation_arithmetic(po.first, val1, val2);
      ctx.i = skip_chars(ctx, SEPARATORS);
    }

    if(po.first == "=" && ttvar!=nullptr) // categorize as var definition
      ttvar->var->definition=true;

    if(ctx.i >= ctx.size)
    {
      parse_error( "Unexpected end of file, expecting '))'", ctx );
      return std::make_pair(ret, ctx);
    }
    if(ctx[ctx.i] != ')')
    {
      parse_error( unexpected_token(ctx[ctx.i])+ ", expecting ')'", ctx);
      return std::make_pair(ret, ctx);
    }
  }

  return std::make_pair(ret, ctx);
}

std::pair<variable*, parse_context> parse_manipulation(parse_context ctx)
{
  variable* ret = nullptr;
  arg* precede = nullptr;
  uint32_t start=ctx.i;


  if(ctx[ctx.i] == '#' || ctx[ctx.i] == '!')
  {
    if(!ctx.bash && ctx[ctx.i] == '!')
    {
      parse_error("bash specific: '${!}'", ctx);
      return std::make_pair(ret, ctx);
    }
    std::string t;
    t+=ctx[ctx.i];
    precede = new arg( t );
    ctx.i++;
  }

  auto p=parse_var(ctx, true, true);
  if(p.first == nullptr)
  {
    parse_error( "Bad variable name", ctx );
    return std::make_pair(ret, ctx);
  }
  ret = p.first;
  ctx = p.second;

  ret->is_manip=true;
  if(precede != nullptr)
  {
    if(ctx[ctx.i] != '}')
    {
      parse_error( "Incompatible operations", ctx, start );
      return std::make_pair(ret, ctx);
    }
    ret->manip = precede;
    ret->precedence=true;
    precede=nullptr;
  }
  else if(ctx[ctx.i] != '}')
  {
    auto pa = parse_arg(ctx, "}", NULL, false);
    ret->manip=pa.first;
    ctx = pa.second;
  }
  ctx.i++;

  return std::make_pair(ret, ctx);
}

parse_context do_one_subarg_step(arg* ret, parse_context ctx, uint32_t& j, bool is_quoted)
{
  if( ctx[ctx.i] == '`' )
  {
    // add previous subarg
    std::string tmpstr=std::string(ctx.data+j, ctx.i-j);
    if(tmpstr!="")
      ret->add(tmpstr);

    ctx.i++;
    uint32_t k=skip_until(ctx, "`");
    if(k>=ctx.size)
    {
      parse_error("Expecting '`'", ctx, ctx.i-1);
      return ctx;
    }
    if(ctx[k-1] == '\\' && ctx[k-2] != '\\')
    {
      parse_error("Escaping backticks is not supported", ctx, k);
      return make_context(ctx, k);
    }
    // get subshell
    parse_context newct = ctx;
    ctx.size=k;
    auto r=parse_list_until(newct);
    ret->add(new subshell_subarg(new subshell(std::get<0>(r)), is_quoted));
    ctx = std::get<1>(r);
    ctx.i++;
    j = ctx.i;
  }
  else if( word_eq("$((", ctx) ) // arithmetic operation
  {
    // add previous subarg
    std::string tmpstr=std::string(ctx.data+j, ctx.i-j);
    if(tmpstr!="")
      ret->add(tmpstr);
    // get arithmetic
    ctx.i+=3;
    auto r=parse_arithmetic(ctx);
    arithmetic_subarg* tt = new arithmetic_subarg(r.first);
    tt->quoted=is_quoted;
    ret->add(tt);
    ctx = r.second;
    if(ctx.i >= ctx.size)
      return ctx;
    if(!word_eq("))", ctx))
    {
      parse_error( "Unexpected token ')', expecting '))'", ctx);
      return ctx+1;
    }
    ctx.i+=2;
    j=ctx.i;
  }
  else if( word_eq("$(", ctx) ) // substitution
  {
    // add previous subarg
    std::string tmpstr=std::string(ctx.data+j, ctx.i-j);
    if(tmpstr!="")
      ret->add(tmpstr);
    // get subshell
    ctx.i+=2;
    auto r=parse_subshell(ctx);
    ret->add(new subshell_subarg(r.first, is_quoted));
    ctx = r.second;
    j = ctx.i;
  }
  else if( word_eq("${", ctx) ) // variable manipulation
  {
    // add previous subarg
    std::string tmpstr=std::string(ctx.data+j, ctx.i-j);
    if(tmpstr!="")
      ret->add(tmpstr);
    // get manipulation
    ctx.i+=2;
    auto r=parse_manipulation(ctx);
    ret->add(new variable_subarg(r.first, is_quoted));
    ctx = r.second;
    j = ctx.i;
  }
  else if( ctx[ctx.i] == '$' )
  {
    parse_context newct=ctx;
    newct.i++;
    auto r=parse_var(newct);
    if(r.first !=nullptr)
    {
      // add previous subarg
      std::string tmpstr=std::string(ctx.data+j, ctx.i-j);
      if(tmpstr!="")
        ret->add(tmpstr);
      // add var
      ret->add(new variable_subarg(r.first, is_quoted));
      ctx = r.second;
      j = ctx.i;
    }
    else
      ctx.i++;
  }
  else
    ctx.i++;
  return ctx;
}

// parse one argument
// must start at a read char
// ends at either " \t|&;\n()"
std::pair<arg*, parse_context> parse_arg(parse_context ctx, const char* end, const char* unexpected, bool doquote)
{
  arg* ret = new arg;
  // j : start of subarg , q = start of quote
  uint32_t j=ctx.i,q=ctx.i;

  if(unexpected != NULL && is_in(ctx[ctx.i], unexpected))
  {
    parse_error( unexpected_token(ctx[ctx.i]), ctx);
  }

  while(ctx.i<ctx.size && !(end != NULL && is_in(ctx[ctx.i], end)) )
  {
    if(ctx.i+1<ctx.size && is_in(ctx[ctx.i], "<>") && ctx[ctx.i+1]=='&') // special case for <& and >&
    {
      ctx.i += 2;
    }
    else if(ctx[ctx.i]=='\\') // backslash: don't check next char
    {
      ctx.i++;
      if(ctx.i>=ctx.size)
        break;
      if(ctx[ctx.i] == '\n') // \ on \n : skip this char
      {
        std::string tmpstr=std::string(ctx.data+j, ctx.i-1-j);
        if(tmpstr!="")
          ret->add(tmpstr);
        ctx.i++;
        j=ctx.i;
      }
      else
        ctx.i++;
    }
    else if(doquote && ctx[ctx.i] == '"') // start double quote
    {
      q=ctx.i;
      ctx.i++;
      while(ctx[ctx.i] != '"') // while inside quoted string
      {
        if(ctx[ctx.i] == '\\') // backslash: don't check next char
        {
          ctx.i+=2;
        }
        else
          ctx = do_one_subarg_step(ret, ctx, j, true);

        if(ctx.i>=ctx.size)
        {
          parse_error("Unterminated double quote", ctx, q);
          return std::make_pair(ret, ctx);
        }
      }
      ctx.i++;
    }
    else if(doquote && ctx[ctx.i] == '\'') // start single quote
    {
      q=ctx.i;
      ctx.i++;
      while(ctx.i<ctx.size && ctx[ctx.i]!='\'')
        ctx.i++;
      if(ctx.i>=ctx.size)
      {
        parse_error("Unterminated single quote", ctx, q);
        return std::make_pair(ret, ctx);
      }
      ctx.i++;
    }
    else
      ctx = do_one_subarg_step(ret, ctx, j, false);
  }

  // add string subarg
  std::string val=std::string(ctx.data+j, ctx.i-j);
  if(val != "")
    ret->add(val);

  return std::make_pair(ret, ctx);
}

parse_context parse_heredocument(parse_context ctx)
{
  if(ctx.here_document == nullptr)
    return ctx;

  uint32_t j=ctx.i;
  char* tc=NULL;
  std::string delimitor=ctx.here_delimitor;
  tc = (char*) strstr(ctx.data+ctx.i, std::string("\n"+delimitor+"\n").c_str()); // find delimitor
  if(tc!=NULL) // delimitor was found
  {
    ctx.i = (tc-ctx.data)+delimitor.size()+1;
  }
  else
  {
    ctx.i = ctx.size;
  }
  // std::string tmpparse=std::string(ctx.data+j, ctx.i-j);
  parse_context newctx = make_context(ctx, j);
  newctx.size = ctx.i;
  auto pval = parse_arg(newctx , NULL, NULL, false);
  ctx.i = pval.second.i;
  ctx.has_errored = pval.second.has_errored;
  ctx.here_document->here_document = pval.first;

  //
  ctx.here_document=nullptr;
  free(ctx.here_delimitor);
  ctx.here_delimitor=NULL;

  return ctx;
}

std::pair<redirect*, parse_context> parse_redirect(parse_context ctx)
{
  bool is_redirect=false;
  bool needs_arg=false;
  bool has_num_prefix=false;

  uint32_t start=ctx.i;

  if(is_num(ctx[ctx.i]))
  {
    ctx.i++;
    has_num_prefix=true;
  }

  if( ctx[ctx.i] == '>' )
  {
    ctx.i++;
    if(ctx.i>ctx.size)
    {
      parse_error("Unexpected end of file", ctx);
      return std::make_pair(nullptr, ctx);
    }
    is_redirect = true;
    if(ctx.i+1<ctx.size && ctx[ctx.i] == '&' && is_num(ctx[ctx.i+1]) )
    {
      ctx.i+=2;
      needs_arg=false;
    }
    else if(ctx[ctx.i] == '&') // >& bash operator
    {
      if(!ctx.bash)
      {
        parse_error("bash specific: '>&'", ctx);
      }
      ctx.i++;
      needs_arg=true;
    }
    else
    {
      if(ctx[ctx.i] == '>')
        ctx.i++;
      needs_arg=true;
    }
  }
  else if( ctx[ctx.i] == '<' )
  {
    if(has_num_prefix)
    {
      parse_error("Invalid input redirection", ctx, ctx.i-1);
    }
    ctx.i++;
    if(ctx.i>ctx.size)
    {
      parse_error("Unexpected end of file", ctx);
      return std::make_pair(nullptr, ctx);
    }
    if(ctx[ctx.i] == '<')
    {
      ctx.i++;
      if(ctx.i<ctx.size && ctx[ctx.i] == '<')
      {
        if(!ctx.bash)
        {
          parse_error("bash specific: '<<<'", ctx);
        }
        ctx.i++;
      }
    }
    is_redirect=true;
    needs_arg=true;
  }
  else if( word_eq("&>", ctx) ) // &> bash operator
  {
    if(!ctx.bash)
    {
      parse_error("bash specific: '&>'", ctx);
    }
    ctx.i+=2;
    if(ctx.i<ctx.size && ctx[ctx.i] == '>')
      ctx.i++;
    is_redirect=true;
    needs_arg=true;
  }


  if(is_redirect)
  {
    redirect* ret=nullptr;

    ret = new redirect;
    ret->op = std::string(ctx.data+start, ctx.i-start);
    if(needs_arg)
    {
      ctx.i = skip_chars(ctx, SPACES);
      if(ret->op == "<<")
      {
        if(ctx.here_document != nullptr)
        {
          parse_error("unsupported multiple here documents at the same time", ctx);
          return std::make_pair(ret, ctx);
        }
        else
          ctx.here_document=ret;

        auto pa = parse_arg(ctx);
        std::string delimitor = pa.first->string();

        if(delimitor == "")
        {
          parse_error("non-static or empty here document delimitor", ctx);
        }

        if(delimitor.find('"') != std::string::npos || delimitor.find('\'') != std::string::npos || delimitor.find('\\') != std::string::npos)
        {
          delimitor = ztd::sh("echo "+delimitor); // shell resolve the delimitor
          delimitor.pop_back(); // remove \n
        }
        ret->target = pa.first;
        ctx = pa.second;
        // copy delimitor
        ctx.here_delimitor = (char*) malloc(delimitor.length()+1);
        strcpy(ctx.here_delimitor, delimitor.c_str());
      }
      else
      {
        auto pa = parse_arg(ctx);
        ret->target = pa.first;
        ctx=pa.second;
      }
    }
    return std::make_pair(ret, ctx);
  }
  else
  {
    ctx.i=start;
    return std::make_pair(nullptr, ctx);
  }
}

// parse one list of arguments (a command for instance)
// must start at a read char
// first char has to be read
// ends at either &|;\n#()
std::pair<arglist*, parse_context> parse_arglist(parse_context ctx, bool hard_error, std::vector<redirect*>* redirs)
{
  arglist* ret = nullptr;

  if(word_eq("[[", ctx, ARG_END) ) // [[ bash specific parsing
  {
    if(!ctx.bash)
    {
      parse_error("bash specific: '[['", ctx);
    }
    while(true)
    {
      if(ret == nullptr)
      ret = new arglist;
      auto pp=parse_arg(ctx, SEPARATORS, NULL);
      ret->add(pp.first);
      ctx = pp.second;
      ctx.i = skip_chars(ctx, SEPARATORS);
      if(word_eq("]]", ctx, ARG_END))
      {
        ret->add(new arg("]]"));
        ctx.i+=2;
        ctx.i = skip_chars(ctx, SPACES);
        if( !is_in(ctx[ctx.i], ARGLIST_END) )
        {
          parse_error("Unexpected argument after ']]'", ctx);
          ctx = parse_arglist(ctx).second;
        }
        break;
      }
      if(ctx.i>=ctx.size)
      {
        parse_error( "Expecting ']]'", ctx);
        return std::make_pair(ret, ctx);
      }
    }
  }
  else if(is_in(ctx[ctx.i], ARGLIST_END) && !word_eq("&>", ctx))
  {
    if(hard_error)
    {
      parse_error( unexpected_token(ctx[ctx.i]) , ctx);
    }
    else
      return std::make_pair(ret, ctx);
  }
  // ** HERE **
  else
  {
    while(ctx.i<ctx.size)
    {
      if(ctx.i+1 < ctx.size && (ctx[ctx.i] == '<' || ctx[ctx.i] == '>') && ctx[ctx.i+1] == '(' ) // bash specific <()
      {
        if(!ctx.bash)
        {
          parse_error(strf("bash specific: %c()", ctx[ctx.i]), ctx);
        }
        bool is_output = ctx[ctx.i] == '>';
        ctx.i+=2;
        if(ret == nullptr)
          ret = new arglist;
        auto ps = parse_subshell(ctx);
        ret->add(new arg(new procsub_subarg(is_output, ps.first)));
        ctx=ps.second;
      }
      else if(redirs!=nullptr)
      {
        auto pr = parse_redirect(ctx);
        if(pr.first != nullptr)
        {
          redirs->push_back(pr.first);
          ctx=pr.second;
        }
        else
          goto argparse;
      }
      else
      {
      argparse:
        if(ret == nullptr)
          ret = new arglist;
        auto pp=parse_arg(ctx);
        ret->add(pp.first);
        ctx = pp.second;
      }
      ctx.i = skip_chars(ctx, SPACES);
      if(word_eq("&>", ctx))
        continue; // &> has to be managed in redirects
      if(word_eq("|&", ctx))
      {
        parse_error("Unsupported '|&', use '2>&1 |' instead", ctx);
        return std::make_pair(ret, ctx+1);
      }
      if(ctx.i>=ctx.size)
        return std::make_pair(ret, ctx);
      if( is_in(ctx[ctx.i], ARGLIST_END) )
        return std::make_pair(ret, ctx);
    }

  }

  return std::make_pair(ret, ctx);
}

// parse a pipeline
// must start at a read char
// separated by |
// ends at either &;\n#)
std::pair<pipeline*, parse_context> parse_pipeline(parse_context ctx)
{
  pipeline* ret = new pipeline;

  if(ctx[ctx.i] == '!' && ctx.i+1<ctx.size && is_in(ctx[ctx.i+1], SPACES))
  {
    ret->negated = true;
    ctx.i++;
    ctx.i=skip_chars(ctx, SPACES);
  }
  while(ctx.i<ctx.size)
  {
    auto pp=parse_block(ctx);
    ret->add(pp.first);
    ctx = pp.second;
    ctx.i = skip_chars(ctx, SPACES);
    if( ctx.i>=ctx.size || is_in(ctx[ctx.i], PIPELINE_END) || word_eq("||", ctx) )
      return std::make_pair(ret, ctx);
    else if( ctx[ctx.i] != '|' )
    {
      parse_error( unexpected_token(ctx[ctx.i] ), ctx);
      return std::make_pair(ret, ctx);
    }
    ctx.i++;
    if(ctx.here_document != nullptr)
    {
      ctx.i = skip_unread_noline(ctx);
      if(ctx[ctx.i] == '\n')
        ctx = parse_heredocument(ctx+1);
    }
    else
      ctx.i = skip_unread(ctx);
  }
  return std::make_pair(ret, ctx);
}

// parse condition lists
// must start at a read char
// separated by && or ||
// ends at either ;\n)#
std::pair<condlist*, parse_context> parse_condlist(parse_context ctx)
{
  condlist* ret = new condlist;
  ctx.i = skip_unread(ctx);

  bool optype=AND_OP;
  while(ctx.i<ctx.size)
  {
    auto pp=parse_pipeline(ctx);
    ret->add(pp.first, optype);
    ctx = pp.second;
    if(ctx.i>=ctx.size || is_in(ctx[ctx.i], CONTROL_END) || is_in(ctx[ctx.i], COMMAND_SEPARATOR)) // end here exactly: used for control later
    {
      return std::make_pair(ret, ctx);
    }
    else if( word_eq("&", ctx) && !word_eq("&&", ctx) ) // parallel: end one char after
    {
      ret->parallel=true;
      ctx.i++;
      return std::make_pair(ret, ctx);
    }
    else if( word_eq("&&", ctx) ) // and op
    {
      ctx.i += 2;
      optype=AND_OP;
    }
    else if( word_eq("||", ctx) ) // or op
    {
      ctx.i += 2;
      optype=OR_OP;
    }
    else
    {
      parse_error( unexpected_token(ctx[ctx.i]), ctx);
      return std::make_pair(ret, ctx);
    }
    if(ctx.here_document != nullptr)
    {
      ctx.i = skip_unread_noline(ctx);
      if(ctx[ctx.i] == '\n')
        ctx = parse_heredocument(ctx+1);
    }
    else
      ctx.i = skip_unread(ctx);
    if(ctx.i>=ctx.size)
    {
      parse_error( "Unexpected end of file", ctx );
      return std::make_pair(ret, ctx);
    }
  }
  return std::make_pair(ret, ctx);
}

std::tuple<list*, parse_context, std::string> parse_list_until(parse_context ctx, list_parse_options opts)
{
  list* ret = new list;
  ctx.i=skip_unread(ctx);
  std::string found_end_word;

  char& end_c = opts.end_char;
  std::vector<std::string>& end_words = opts.end_words;

  const char* old_expect=ctx.expecting;

  if(opts.expecting!=NULL)
    ctx.expecting=opts.expecting;
  else if(opts.word_mode)
    ctx.expecting=end_words[0].c_str();
  else
    ctx.expecting=std::string(&end_c, 1).c_str();

  bool stop=false;
  while(true)
  {
    if(opts.word_mode)
    {
      // check words
      auto wp=get_word(ctx, ARG_END);
      for(auto it: end_words)
      {
        if(it == ";" && ctx[ctx.i] == ';')
        {
          found_end_word=";";
          ctx.i++;
          stop=true;
          break;
        }
        if(wp.first == it)
        {
          found_end_word=it;
          ctx.i=wp.second;
          stop=true;
          break;
        }
      }
      if(stop)
        break;
    }
    else if(ctx[ctx.i] == end_c)
    {
      break;
    }
    // do a parse
    auto pp=parse_condlist(ctx);
    ret->add(pp.first);
    ctx=pp.second;

    if(!opts.word_mode && ctx[ctx.i] == end_c)
      break; // reached end char: stop here
    else if(ctx[ctx.i] == '\n')
    {
      if(ctx.here_document != nullptr)
        ctx = parse_heredocument(ctx+1);
      // do here document parse
    }
    else if(ctx[ctx.i] == '#')
      ; // skip here
    else if(is_in(ctx[ctx.i], COMMAND_SEPARATOR))
      ; // skip on next
    else if(is_in(ctx[ctx.i], CONTROL_END))
    {
      // control end: unexpected
      parse_error( unexpected_token(ctx[ctx.i]), ctx);
      break;
    }

    if(ctx.here_document != nullptr)
    {
      uint8_t do_twice=2;
      // case of : cat << EOF ;
      while(do_twice>0)
      {
        if(ctx[ctx.i] == '\n')
        {
          ctx = parse_heredocument(ctx+1);
          break;
        }
        else if(ctx[ctx.i] == '#')
        {
          ctx.i = skip_until(ctx, "\n"); //skip to endline
          ctx = parse_heredocument(ctx+1);
          break;
        }
        skip_chars(ctx, SPACES);
        do_twice--;
      }
      // case of : cat << EOF ; ;
      if(do_twice==0 && is_in(ctx[ctx.i], COMMAND_SEPARATOR))
        parse_error( unexpected_token(ctx[ctx.i]), ctx);
    }

    if(is_in(ctx[ctx.i], COMMAND_SEPARATOR))
      ctx.i++;

    ctx.i = skip_unread(ctx);

    // word wasn't found
    if(ctx.i>=ctx.size)
    {
      if(opts.word_mode || opts.end_char != 0)
      {
        parse_error(strf("Expecting '%s'", ctx.expecting), ctx);
        return std::make_tuple(ret, ctx, "");
      }
      else
        break;
    }
  }
  ctx.expecting=old_expect;
  return std::make_tuple(ret, ctx, found_end_word);
}

// parse a subshell
// must start right after the opening (
// ends at ) and nothing else
std::pair<subshell*, parse_context> parse_subshell(parse_context ctx)
{
  subshell* ret = new subshell;
  uint32_t start=ctx.i;
  ctx.i = skip_unread(ctx);

  auto pp=parse_list_until(ctx, {.end_char=')', .expecting=")"} );
  ret->lst=std::get<0>(pp);
  ctx=std::get<1>(pp);
  if(ret->lst->size()<=0)
  {
    parse_error("Subshell is empty", ctx, start-1);
  }
  ctx.i++;

  return std::make_pair(ret,ctx);
}


// parse a brace block
// must start right after the opening {
// ends at } and nothing else
std::pair<brace*, parse_context> parse_brace(parse_context ctx)
{
  brace* ret = new brace;
  uint32_t start=ctx.i;
  ctx.i = skip_unread(ctx);

  auto pp=parse_list_until(ctx, {.end_char='}', .expecting="}"});
  ret->lst=std::get<0>(pp);
  ctx=std::get<1>(pp);
  if(ret->lst->size()<=0)
  {
    parse_error("Brace block is empty", ctx, start-1);
    return std::make_pair(ret, ctx+1);
  }
  ctx.i++;

  return std::make_pair(ret,ctx);
}

// parse a function
// must start right after the ()
// then parses a brace block
std::pair<function*, parse_context> parse_function(parse_context ctx, const char* after)
{
  function* ret = new function;

  ctx.i=skip_unread(ctx);
  if(ctx[ctx.i] != '{')
  {
    parse_error( strf("Expecting { after %s", after) , ctx);
    return std::make_pair(ret, ctx);
  }
  ctx.i++;

  auto pp=parse_list_until(ctx, {.end_char='}', .expecting="}"} );
  ret->lst=std::get<0>(pp);
  if(ret->lst->size()<=0)
  {
    parse_error("Function is empty", ctx);
    ctx.i=std::get<1>(pp).i+1;
    return std::make_pair(ret, ctx);
  }

  ctx=std::get<1>(pp);
  ctx.i++;

  return std::make_pair(ret, ctx);
}

// parse only var assigns
parse_context parse_cmd_varassigns(cmd* in, parse_context ctx, bool cmdassign=false, std::string const& cmd="")
{
  bool forbid_assign=false;
  bool forbid_special=false;
  if(cmdassign && (cmd == "read" || cmd == "unset") )
    forbid_assign=true;
  if(cmdassign && (forbid_special || cmd == "export") )
    forbid_special=true;

  std::vector<std::pair<variable*,arg*>>* ret=&in->var_assigns;
  if(cmdassign)
    ret=&in->cmd_var_assigns;

  while(ctx.i<ctx.size && !is_in(ctx[ctx.i], ARGLIST_END))
  {
    auto vp=parse_var(ctx, false, true);
    if(vp.first != nullptr)
      vp.first->definition=true;
    parse_context newct = vp.second;
    if(newct.has_errored)
      ctx.has_errored=true;
    if(vp.first != nullptr && newct.i<newct.size && (newct[newct.i] == '=' || word_eq("+=", newct) )) // is a var assign
    {
      if(forbid_assign)
      {
        parse_error("Unallowed assign", newct);
      }
      std::string strop = "=";
      ctx = newct;
      if( word_eq("+=", ctx) ) // bash var+=
      {
        if(!ctx.bash)
        {
          parse_error("bash specific: var+=", ctx);
        }
        if(forbid_special)
        {
          parse_error("Unallowed special assign", ctx);
        }
        strop = "+=";
        ctx.i+=2;
      }
      else
        ctx.i++;

      arg* ta=nullptr;
      if(ctx[ctx.i] == '(') // bash var=()
      {
        if(!ctx.bash)
        {
          parse_error("bash specific: var=()", ctx);
        }
        if(forbid_special)
        {
          parse_error("Unallowed special assign", ctx);
        }
        ctx.i++;
        auto pp=parse_arg(ctx, ")");
        ta=pp.first;
        ta->insert(0,"(");
        ta->add(")");
        ctx = pp.second;
        ctx.i++;
      }
      else if( is_in(ctx[ctx.i], ARG_END) ) // no value : give empty value
      {
        ta = new arg;
      }
      else
      {
        auto pp=parse_arg(ctx);
        ta=pp.first;
        ctx=pp.second;
      }
      ta->insert(0, strop);
      ret->push_back(std::make_pair(vp.first, ta));
      ctx.i=skip_chars(ctx, SPACES);
    }
    else
    {
      if(cmdassign)
      {
        if(vp.first != nullptr && is_in(newct[newct.i], ARG_END) )
        {
          ret->push_back(std::make_pair(vp.first, nullptr));
          ctx=newct;
        }
        else
        {
          delete vp.first;
          auto pp=parse_arg(ctx);
          ret->push_back(std::make_pair(nullptr, pp.first));
          ctx=pp.second;
        }
        ctx.i=skip_chars(ctx, SPACES);
      }
      else
      {
        if(vp.first != nullptr)
          delete vp.first;
        break;
      }
    }
  }
  return ctx;
}

// must start at read char
std::pair<cmd*, parse_context> parse_cmd(parse_context ctx)
{
  cmd* ret = new cmd;

  ctx = parse_cmd_varassigns(ret, ctx);

  auto wp=get_word(ctx, ARG_END);
  bool is_bash_cmdvar=false;
  if(is_in_set(wp.first, posix_cmdvar) || (is_bash_cmdvar=is_in_set(wp.first, bash_cmdvar)) )
  {
    if(!ctx.bash && (is_bash_cmdvar || is_in_set(wp.first, bash_cmdvar)))
    {
      parse_error("bash specific: "+wp.first, ctx);
    }

    ret->args = new arglist;
    ret->args->add(new arg(wp.first));
    ret->is_cmdvar=true;
    ctx.i = wp.second;
    ctx.i = skip_chars(ctx, SPACES);

    ctx = parse_cmd_varassigns(ret, ctx, true, wp.first);
  }
  else if(!is_in(ctx[ctx.i], ARGLIST_END))
  {
    auto pp=parse_arglist(ctx, true, &ret->redirs);
    ret->args = pp.first;
    ctx = pp.second;
  }
  else if( ret->var_assigns.size() <= 0 )
  {
    parse_error( unexpected_token(ctx[ctx.i]), ctx );
    ctx.i++;
  }

  return std::make_pair(ret, ctx);
}

// parse a case block
// must start right after the case
// ends at } and nothing else
std::pair<case_block*, parse_context> parse_case(parse_context ctx)
{
  case_block* ret = new case_block;
  ctx.i=skip_chars(ctx, SPACES);

  // get the treated argument
  auto pa = parse_arg(ctx);
  ret->carg = pa.first;
  ctx=pa.second;
  ctx.i=skip_unread(ctx);

  // must be an 'in'
  if(!word_eq("in", ctx, SEPARATORS))
  {
    std::string word=get_word(ctx, SEPARATORS).first;
    parse_error( strf("Unexpected word: '%s', expecting 'in' after case", word.c_str()), ctx);
  }
  ctx.i+=2;
  ctx.i=skip_unread(ctx);

  // parse all cases
  while(ctx.i<ctx.size && !word_eq("esac", ctx, ARG_END) )
  {
    // add one element
    ret->cases.push_back( std::make_pair(std::vector<arg*>(), nullptr) );
    // iterator to last element
    auto cc = ret->cases.end()-1;

    // toto)
    while(true)
    {
      pa = parse_arg(ctx);
      cc->first.push_back(pa.first);
      ctx = pa.second;
      if(pa.first->size() <= 0)
      {
        parse_error("Empty case value", ctx);
      }
      ctx.i = skip_unread(ctx);
      if(ctx.i>=ctx.size)
      {
        parse_error("Unexpected end of file. Expecting 'esac'", ctx);
        return std::make_pair(ret, ctx);
      }
      if(ctx[ctx.i] == ')')
        break;
      if(is_in(ctx[ctx.i], PIPELINE_END))
      {
        parse_error( unexpected_token(ctx[ctx.i])+", expecting ')'", ctx );
      }
      // |
      ctx.i++;
      ctx.i=skip_unread(ctx);
    }
    ctx.i++;

    // until ;;
    auto tp = parse_list_until(ctx, { .word_mode=true, .end_words={";", "esac"}, .expecting=";;" });
    cc->second = std::get<0>(tp);
    ctx = std::get<1>(tp);
    std::string word = std::get<2>(tp);
    if(word == "esac")
    {
      ctx.i -= 4;
      break;
    }
    if(ctx.i >= ctx.size)
    {
      parse_error("Expecting ';;'", ctx);
    }
    if(ctx[ctx.i-1] != ';')
    {
      parse_error(strf("Unexpected token '%c'", ctx[ctx.i-1]), ctx);
    }
    if(ctx[ctx.i] == ';')
      ctx.i++;
    ctx.i=skip_unread(ctx);
  }

  // ended before finding esac
  if(ctx.i>=ctx.size)
  {
    parse_error("Expecting 'esac'", ctx);
    return std::make_pair(ret, ctx);
  }
  ctx.i+=4;

  return std::make_pair(ret, ctx);
}

std::pair<if_block*, parse_context> parse_if(parse_context ctx)
{
  if_block* ret = new if_block;

  while(true)
  {
    std::string word;
    parse_context oldctx = ctx;

    ret->blocks.push_back(std::make_pair(nullptr, nullptr));
    auto ll = ret->blocks.end()-1;

    auto pp=parse_list_until(ctx, {.word_mode=true, .end_words={"then"}});
    ll->first = std::get<0>(pp);
    ctx = std::get<1>(pp);
    if(ll->first->size()<=0)
    {
      parse_error("Condition is empty", oldctx);
      ctx.has_errored=true;
    }

    auto tp=parse_list_until(ctx, {.word_mode=true, .end_words={"fi", "elif", "else"}} );
    ll->second = std::get<0>(tp);
    parse_context newctx = std::get<1>(tp);
    word = std::get<2>(tp);
    if(ll->second->size() <= 0)
    {
      parse_error("if block is empty", ctx);
      newctx.has_errored=true;
    }
    ctx = newctx;
    if(ctx.i >= ctx.size)
    {
      return std::make_pair(ret, ctx);
    }

    if(word == "fi")
      break;
    if(word == "else")
    {
      auto pp=parse_list_until(ctx, {.word_mode=true, .end_words={"fi"}});
      ret->else_lst=std::get<0>(pp);
      if(ret->else_lst->size()<=0)
      {
        parse_error("else block is empty", ctx);
        ctx=std::get<1>(pp);
        ctx.has_errored=true;
      }
      else
        ctx=std::get<1>(pp);

      break;
    }

  }

  return std::make_pair(ret, ctx);
}

std::pair<for_block*, parse_context> parse_for(parse_context ctx)
{
  for_block* ret = new for_block;
  ctx.i = skip_chars(ctx, SPACES);

  auto wp = get_word(ctx, ARG_END);

  if(!valid_name(wp.first))
  {
    parse_error( strf("Bad variable name in for clause: '%s'", wp.first.c_str()), ctx );
  }
  ret->var = new variable(wp.first, nullptr, true);
  ctx.i = wp.second;
  ctx.i=skip_chars(ctx, SPACES);

  // in
  wp = get_word(ctx, ARG_END);
  if(wp.first == "in")
  {
    ctx.i=wp.second;
    ctx.i=skip_chars(ctx, SPACES);
    auto pp = parse_arglist(ctx, false);
    ret->iter = pp.first;
    ctx = pp.second;
    ret->in_val=true;
  }
  else if(wp.first != "")
  {
    parse_error( "Expecting 'in' after for", ctx );
    ctx.i=wp.second;
    ctx.i=skip_chars(ctx, SPACES);
  }

  // end of arg list
  if(!is_in(ctx[ctx.i], "\n;#"))
  {
    parse_error( unexpected_token(ctx[ctx.i])+", expecting newline, ';' or 'in'", ctx );
    while(!is_in(ctx[ctx.i], "\n;#"))
      ctx.i++;
  }
  if(ctx[ctx.i] == ';')
    ctx.i++;
  ctx.i=skip_unread(ctx);

  // do
  wp = get_word(ctx, ARG_END);
  if(wp.first != "do")
  {
    parse_error( "Expecting 'do', after for", ctx);
  }
  else
  {
    ctx.i = wp.second;
    ctx.i = skip_unread(ctx);
  }

  // ops
  auto lp = parse_list_until(ctx, {.word_mode=true, .end_words={"done"}} );
  ret->ops=std::get<0>(lp);
  ctx=std::get<1>(lp);

  return std::make_pair(ret, ctx);
}

std::pair<while_block*, parse_context> parse_while(parse_context ctx)
{
  while_block* ret = new while_block;

  // cond
  parse_context oldctx = ctx;
  auto pp=parse_list_until(ctx, {.word_mode=true, .end_words={"do"}});

  ret->cond = std::get<0>(pp);
  ctx = std::get<1>(pp);

  if(ret->cond->size() <= 0)
  {
    parse_error("condition is empty", oldctx);
    ctx.has_errored=true;
  }

  // ops
  oldctx = ctx;
  auto lp = parse_list_until(ctx, {.word_mode=true, .end_words={"done"}} );
  ret->ops=std::get<0>(lp);
  ctx = std::get<1>(lp);
  if(ret->ops->size() <= 0)
  {
    parse_error("while is empty", oldctx);
    ctx.has_errored=true;
  }

  return std::make_pair(ret, ctx);
}

// detect if brace, subshell, case or other
std::pair<block*, parse_context> parse_block(parse_context ctx)
{
  ctx.i = skip_chars(ctx, SEPARATORS);
  block* ret = nullptr;

  if(ctx.i>=ctx.size)
  {
    parse_error("Unexpected end of file", ctx);
    return std::make_pair(ret, ctx);
  }
  if( ctx.data[ctx.i] == '(' ) //subshell
  {
    ctx.i++;
    auto pp = parse_subshell(ctx);
    ret = pp.first;
    ctx = pp.second;
  }
  else
  {
    auto wp=get_word(ctx, BLOCK_TOKEN_END);
    std::string& word=wp.first;
    parse_context newct=ctx;
    newct.i=wp.second;
    // reserved words
    if( word == "{" ) // brace block
    {
      auto pp = parse_brace(newct);
      ret = pp.first;
      ctx = pp.second;
    }
    else if(word == "case") // case
    {
      auto pp = parse_case(newct);
      ret = pp.first;
      ctx = pp.second;
    }
    else if( word == "if" ) // if
    {
      auto pp=parse_if(newct);
      ret = pp.first;
      ctx = pp.second;
    }
    else if( word == "for" )
    {
      auto pp=parse_for(newct);
      ret = pp.first;
      ctx = pp.second;
    }
    else if( word == "while" )
    {
      auto pp=parse_while(newct);
      ret = pp.first;
      ctx = pp.second;
    }
    else if( word == "until" )
    {
      auto pp=parse_while(newct);
      pp.first->real_condition()->negate();
      ret = pp.first;
      ctx = pp.second;
    }
    else if(is_in_set(word, out_reserved_words)) // is a reserved word
    {
      parse_error( strf("Unexpected '%s'", word.c_str())+expecting(ctx.expecting) , ctx);
      ctx.i+=word.size();
    }
    // end reserved words
    else if( word == "function" ) // bash style function
    {
      if(!ctx.bash)
      {
        parse_error("bash specific: 'function'", ctx);
        newct.has_errored=true;
      }
      newct.i = skip_unread(newct);
      auto wp2=get_word(newct, BASH_BLOCK_END);
      if(!valid_name(wp2.first))
      {
        parse_error( strf("Bad function name: '%s'", wp2.first.c_str()), newct );
      }

      newct.i = wp2.second;
      newct.i=skip_unread(newct);
      if(word_eq("()", newct))
      {
        newct.i+=2;
        newct.i=skip_unread(newct);
      }

      auto pp = parse_function(newct, "function definition");
      // function name
      pp.first->name = wp2.first;
      ret = pp.first;
      ctx = pp.second;
    }
    else if(word_eq("()", ctx.data, ctx.size, skip_unread(ctx.data, ctx.size, wp.second))) // is a function
    {
      if(!valid_name(word))
      {
        parse_error( strf("Bad function name: '%s'", word.c_str()), ctx );
        newct.has_errored=true;
      }

      newct.i = skip_unread(ctx.data, ctx.size, wp.second)+2;
      auto pp = parse_function(newct);
      // first arg is function name
      pp.first->name = word;
      ret = pp.first;
      ctx = pp.second;
    }
    else // is a command
    {
      auto pp = parse_cmd(ctx);
      ret = pp.first;
      ctx = pp.second;
    }

  }

  if(ret!=nullptr && ret->type != block::block_cmd)
  {
    uint32_t j=skip_chars(ctx, SPACES);
    ctx.i=j;
    auto pp=parse_arglist(ctx, false, &ret->redirs); // in case of redirects
    if(pp.first != nullptr)
    {
      delete pp.first;
      parse_error("Extra argument after block", ctx);
      pp.second.has_errored=true;
    }
    ctx=pp.second;
  }

  return std::make_pair(ret,ctx);
}

// parse main
std::pair<shmain*, parse_context> parse_text(parse_context ctx)
{
  shmain* ret = new shmain();

  ret->filename=ctx.filename;
  // get shebang
  if(word_eq("#!", ctx))
  {
    ctx.i=skip_until(ctx, "\n");
    ret->shebang=std::string(ctx.data, ctx.i);
  }
  ctx.i = skip_unread(ctx);
  // do bash reading
  std::string binshebang = basename(ret->shebang);
  if(!ctx.bash)
    ctx.bash = (binshebang == "bash" || binshebang == "lxsh");
  // parse all commands
  auto pp=parse_list_until(ctx);
  ret->lst=std::get<0>(pp);
  ctx = std::get<1>(pp);

  if(ctx.has_errored)
    throw std::runtime_error("Aborted due to previous errors");

  return std::make_pair(ret, ctx);
}

std::pair<shmain*, parse_context> parse_text(std::string const& in, std::string const& filename)
{
  return parse_text({ .data=in.c_str(), .size=in.size(), .filename=filename.c_str()});
}

// import a file's contents into a string
std::string import_file(std::string const& path)
{
  std::ifstream st(path);
  if(!st)
    throw std::runtime_error("Cannot open stream to '"+path+'\'');

  std::string ret, ln;
  while(getline(st, ln))
  {
    ret += ln + '\n';
  }
  st.close();
  return ret;
}
