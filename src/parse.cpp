#include "parse.hpp"

#include <fstream>
#include <strings.h>
#include <string.h>

#include "util.hpp"

std::string g_origin;

inline bool is_in(char c, const char* set)
{
  return strchr(set, c) != NULL;
}

inline bool is_alphanum(char c)
{
  return (c >= 'a' && c<='z') || (c >= 'A' && c<='Z') || (c >= '0' && c<='9');
}
inline bool is_alpha(char c)
{
  return (c >= 'a' && c<='z') || (c >= 'A' && c<='Z');
}

bool word_eq(const char* word, const char* in, uint32_t size, uint32_t start, const char* end_set=NULL)
{
  uint32_t wordsize=strlen(word);
  if(wordsize > size-start)
    return false;
  if(strncmp(word, in+start, wordsize) == 0)
  {
    if(end_set==NULL)
      return true;
    // end set
    if(wordsize < size-start)
      return is_in(in[start+wordsize], end_set);
  }
  return false;
}

std::string get_word(const char* in, uint32_t size, uint32_t start, const char* end_set)
{
  uint32_t i=start;
  while(i<size && !is_in(in[i], end_set))
    i++;

  return std::string(in+start, i-start);
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
    i = skip_chars(in, size, i, " \t\n");
    if(in[i] != '#') // not a comment
      return i;
    i = skip_until(in, size, i, "\n"); //skip to endline
  }
}

std::pair<block, uint32_t> parse_subshell(const char* in, uint32_t size, uint32_t start);

// parse an arithmetic
// ends at ))
// for now, uses subshell parsing then takes raw string value
// temporary, to improve
std::pair<subarg, uint32_t> parse_arithmetic(const char* in, uint32_t size, uint32_t start)
{
  subarg ret(subarg::arithmetic);
  uint32_t i=start;

  auto pp=parse_subshell(in, size, i);
  i=pp.second;
  if(i >= size || in[i]!=')')
    throw ztd::format_error( "Unexpected token ')', expecting '))'", g_origin, in, i );
  ret.val = std::string(in+start, i-start-1);
  i++;

  return std::make_pair(ret, i);
}

// parse one argument
// must start at a read char
// ends at either " \t|&;\n()"
std::pair<arg, uint32_t> parse_arg(const char* in, uint32_t size, uint32_t start)
{
  arg ret;
  // j : start of subarg
  uint32_t i=start,j=start,q=start;

  if(is_in(in[i], "&|;\n#()"))
    throw ztd::format_error( strf("Unexpected token '%c'", in[i]) , g_origin, in, i);

  while(i<size && !is_in(in[i], " \t|&;\n()"))
  {
    if(i+1<size && is_in(in[i], "<>") && in[i+1]=='&') // special case for <& and >&
    {
      i+=2;
    }
    else if(in[i]=='\\') // backslash: don't check next char
    {
      i++;
      if(i>=size)
        break;
      i++;
    }
    else if(in[i] == '"') // start double quote
    {
      q=i;
      i++;
      while(in[i] != '"') // while inside quoted string
      {
        if(in[i] == '\\') // backslash: don't check next char
        {
          i+=2;
        }
        else if( word_eq("$((", in, size, i) ) // arithmetic operation
        {
          // add previous subarg
          ret.sa.push_back(subarg(std::string(in+j, i-j)));
          i+=3;
          // get arithmetic
          auto r=parse_arithmetic(in, size, i);
          ret.sa.push_back(r.first);
          j = i = r.second;
        }
        else if( word_eq("$(", in, size, i) ) // substitution
        {
          // add previous subarg
          ret.sa.push_back(subarg(std::string(in+j, i-j)));
          i+=2;
          // get subshell
          auto r=parse_subshell(in, size, i);
          ret.sa.push_back(subarg(r.first));
          j = i = r.second;
        }
        else
          i++;

        if(i>=size)
          throw ztd::format_error("Unterminated double quote", g_origin, in, q);
      }
      i++;
    }
    else if(in[i] == '\'') // start single quote
    {
      q=i;
      i++;
      while(i<size && in[i]!='\'')
        i++;
      if(i>=size)
        throw ztd::format_error("Unterminated single quote", g_origin, in, q);
      i++;
    }
    else if( word_eq("$((", in, size, i) ) // arithmetic operation
    {
      // add previous subarg
      ret.sa.push_back(subarg(std::string(in+j, i-j)));
      i+=3;
      // get arithmetic
      auto r=parse_arithmetic(in, size, i);
      ret.sa.push_back(r.first);
      j = i = r.second;
    }
    else if( word_eq("$(", in, size, i) ) // substitution
    {
      // add previous subarg
      ret.sa.push_back(subarg(std::string(in+j, i-j)));
      i+=2;
      // get subshell
      auto r=parse_subshell(in, size, i);
      ret.sa.push_back(subarg(r.first));
      j = i = r.second;
    }
    else
      i++;
  }

  // add string subarg
  std::string val=std::string(in+j, i-j);
  ret.sa.push_back(subarg(val));

  // raw string for other uses
  ret.raw = std::string(in+start, i-start);

  return std::make_pair(ret, i);
}

// parse one list of arguments (a command for instance)
// must start at a read char
// first char has to be read
// ends at either &|;\n#()
std::pair<arglist, uint32_t> parse_arglist(const char* in, uint32_t size, uint32_t start, bool hard_error=false)
{
  uint32_t i=start;
  arglist ret;
  if(is_in(in[i], "&|;\n#(){}"))
  {
    if(hard_error)
      throw ztd::format_error( strf("Unexpected token '%c'", in[i]) , g_origin, in, i);
    else
      return std::make_pair(ret, i);
  }
  while(i<size)
  {
    auto pp=parse_arg(in, size, i);
    ret.args.push_back(pp.first);
    i = skip_chars(in, size, pp.second, " \t");
    if(i>=size || is_in(in[i], "&|;\n#()") )
      return std::make_pair(ret, i);
  }
  return std::make_pair(ret, i);
}

std::pair<block, uint32_t> parse_block(const char* in, uint32_t size, uint32_t start);

// parse a pipeline
// must start at a read char
// separated by |
// ends at either &;\n#)
std::pair<pipeline, uint32_t> parse_pipeline(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  pipeline ret;
  while(i<size)
  {
    auto pp=parse_block(in, size, i);
    ret.add(pp.first);
    i = skip_chars(in, size, pp.second, " \t");
    if( (i>=size || is_in(in[i], "&;\n#)") ) || word_eq("||", in, size, i) )
      return std::make_pair(ret, i);
    else if( in[i] != '|')
      throw ztd::format_error( strf("Unexpected token: '%c'", in[i] ), g_origin, in, i);
    i++;
  }
  return std::make_pair(ret, i);
}

// parse condition lists
// must start at a read char
// separated by && or ||
// ends at either ;\n)#
std::pair<condlist, uint32_t> parse_condlist(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  condlist ret;
  bool optype=AND_OP;
  while(i<size)
  {
    auto pp=parse_pipeline(in, size, i);
    ret.add(pp.first, optype);
    i = pp.second;
    if(i>=size || is_in(in[i], ")#")) // end here exactly: used for control later
    {
      return std::make_pair(ret, i);
    }
    else if(is_in(in[i], ";\n")) // end one char after: skip them for next parse
    {
      i++;
      return std::make_pair(ret, i);
    }
    else if( word_eq("&", in, size, i) && !word_eq("&&", in, size, i) ) // parallel: end one char after
    {
      ret.parallel=true;
      i++;
      return std::make_pair(ret, i);
    }
    else if( word_eq("&&", in, size, i) ) // and op
    {
      i += 2;
      optype=AND_OP;
    }
    else if( word_eq("||", in, size, i) ) // or op
    {
      i += 2;
      optype=OR_OP;
    }
    else if(i<size-1) // bad combination
      throw ztd::format_error( strf("Unexpected token: '%c%c'", in[i], in[i+1]), g_origin, in, i);
    else // unknown
      throw ztd::format_error("Unknown error", g_origin, in, i);
    i = skip_unread(in, size, i);
    if(i>=size)
      throw ztd::format_error( "Unexpected end of file", g_origin, in, i );
  }
  return std::make_pair(ret, i);
}

// parse a subshell
// must start right after the opening (
// ends at ) and nothing else
std::pair<block, uint32_t> parse_subshell(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  block ret(block::subshell);
  while(in[i] != ')')
  {
    auto pp=parse_condlist(in, size, i);
    ret.cls.push_back(pp.first);
    i = skip_unread(in, size, pp.second);
    if(i>=size)
      throw ztd::format_error("Expecting )", g_origin, in, start-1);
  }
  if(ret.cls.size()<=0)
    throw ztd::format_error("Subshell is empty", g_origin, in, start-1);
  i++;
  return std::make_pair(ret,i);
}

// parse a brace block
// must start right after the opening {
// ends at } and nothing else
std::pair<block, uint32_t> parse_brace(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  block ret(block::brace);
  while(in[i] != '}')
  {
    auto pp=parse_condlist(in, size, i);
    ret.cls.push_back(pp.first);
    i = skip_unread(in, size, pp.second);
    if(i>=size)
      throw ztd::format_error("Expecting }", g_origin, in, start-1);
    if(is_in(in[i], ")"))
      throw ztd::format_error( strf("Unexpected token: '%c'", in[i]) , g_origin, in, i );
  }
  if(ret.cls.size()<=0)
    throw ztd::format_error("Brace block is empty", g_origin, in, start-1);
  i++;

  return std::make_pair(ret,i);
}

// parse a functions
// must start right after the ()
// then parses a brace block
std::pair<block, uint32_t> parse_function(const char* in, uint32_t size, uint32_t start)
{
  block ret(block::function);
  uint32_t i=start;

  i=skip_unread(in, size, i);
  if(in[i] != '{')
    throw ztd::format_error("Expecting { after ()", g_origin, in, i);

  i++;
  auto pp = parse_brace(in, size, i);
  ret.cls = pp.first.cls;
  i=pp.second;

  return std::make_pair(ret, i);
}

std::pair<block, uint32_t> parse_cmd(const char* in, uint32_t size, uint32_t start)
{
  block ret(block::cmd);
  uint32_t i=start;

  // parse first arg and keep it
  auto tp=parse_arg(in, size, i);
  i=skip_unread(in, size, tp.second);
  if(word_eq("()", in, size, i)) // is a function
  {
    i += 2;
    auto pp = parse_function(in, size, i);
    // first arg is function name
    pp.first.shebang = tp.first.raw;
    return pp;
  }
  else // is a command
  {
    auto pp=parse_arglist(in, size, start, true);
    ret.args = pp.first;
    i = pp.second;
  }

  return std::make_pair(ret, i);
}

// parse a case block
// must start right after the case
// ends at } and nothing else
std::pair<block, uint32_t> parse_case(const char* in, uint32_t size, uint32_t start)
{
  block ret(block::case_block);
  uint32_t i=skip_chars(in, size, start, " \t");;

  // get the treated argument
  auto pa = parse_arg(in, size, i);
  ret.carg = pa.first;
  i=skip_unread(in, size, pa.second);

  // must be an 'in'
  if(!word_eq("in", in, size, i, " \t\n"))
  {
    std::string pp=get_word(in, size, i, " \t\n");
    throw ztd::format_error("Unexpected word: '"+pp+"', expecting 'in' after case", g_origin, in, i);
  }

  i=skip_unread(in, size, i+2);

  // parse all cases
  while(i<size && !word_eq("esac", in, size, i, " \t\n;()&") )
  {
    // toto)
    std::pair<arg, list_t> cc;
    pa = parse_arg(in, size, i);
    if(pa.first.raw == "")
      throw ztd::format_error("Empty case value", g_origin, in, i);
    cc.first = pa.first;
    i=skip_unread(in, size, pa.second);

    if(in[i] != ')')
      throw ztd::format_error( strf("Unexpected token '%c', expecting ')'", in[i]), g_origin, in, i );
    i++;

    while(true) // blocks
    {
      auto pc = parse_condlist(in, size, i);
      cc.second.push_back(pc.first);
      i=pc.second;

      if(i+1>=size)
        throw ztd::format_error("Expecting ';;'", g_origin, in, i);
      if(in[i] == ')')
        throw ztd::format_error( strf("Unexpected token '%c', expecting ';;'", in[i]), g_origin, in, i );

      // end of case: on same line
      if(in[i-1] == ';' && in[i] == ';')
      {
        i++;
        break;
      }

      // end of case: on new line
      i=skip_unread(in, size, i);
      if(word_eq(";;", in, size, i))
      {
        i+=2;
        break;
      }
      // end of block: ignore missing ;;
      if(word_eq("esac", in, size, i))
        break;

    }
    i=skip_unread(in, size, i);
    ret.cases.push_back(cc);
  }

  // ended before finding esac
  if(i>=size)
    throw ztd::format_error("Expecting 'esac'", g_origin, in, i);
  i+=4;

  return std::make_pair(ret, i);
}

// detect if brace, subshell, case or other
std::pair<block, uint32_t> parse_block(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_chars(in, size, start, " \n\t");
  if(i>=size)
    throw ztd::format_error("Unexpected end of file", g_origin, in, i);
  std::pair<block, uint32_t> ret;
  if(in[i] == '{') // brace block
  {
    i++;
    ret = parse_brace(in, size, i);
  }
  else if(in[i] == '(') //subshell
  {
    i++;
    ret = parse_subshell(in, size, i);
  }
  else if(word_eq("case", in, size, i))
  {
    ret = parse_case(in, size, i+4);
  }
  else // command
  {
    ret = parse_cmd(in, size, i);
  }
  if(ret.first.args.args.size()<=0)
  {
    auto pp=parse_arglist(in, size, ret.second, false); // in case of redirects
    ret.second=pp.second;
    ret.first.args=pp.first;
  }
  return ret;
}

// parse main
block parse(const char* in, uint32_t size)
{
  block ret(block::main);
  uint32_t i=0;
  // get shebang
  if(word_eq("#!", in, size, 0))
  {
    i=skip_until(in, size, 0, "\n");
    ret.shebang=std::string(in, i);
  }
  i = skip_unread(in, size, i);
  // parse all commands
  while(i<size)
  {
    auto pp=parse_condlist(in, size, i);
    ret.cls.push_back(pp.first);
    i = skip_unread(in, size, pp.second);
  }
  return ret;
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
