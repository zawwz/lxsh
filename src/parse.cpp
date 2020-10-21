#include "parse.hpp"

#include <fstream>
#include <strings.h>
#include <string.h>

#include "util.hpp"

std::string g_origin;

const std::vector<std::string> reserved_words = { "if", "then", "else", "fi", "case", "esac", "for", "while", "until", "do", "done", "{", "}" };

std::string g_expecting;

std::string expecting(std::string const& word)
{
  if(word != "")
    return ", expecting '"+word+"'";
  else
    return "";
}

// basic char utils

inline bool is_in(char c, const char* set)
{
  return strchr(set, c) != NULL;
}

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

inline bool is_alphanum(char c)
{
  return (c >= 'a' && c<='z') || (c >= 'A' && c<='Z') || (c >= '0' && c<='9');
}
inline bool is_alpha(char c)
{
  return (c >= 'a' && c<='z') || (c >= 'A' && c<='Z');
}

bool is_alphanum(std::string const& str)
{
  for(auto it: str)
  {
    if(! (is_alphanum(it) || it=='_' ) )
      return false;
  }
  return true;
}

bool valid_name(std::string const& str)
{
  return (is_alpha(str[0]) || str[0] == '_') && is_alphanum(str);
}

// string utils

bool word_is_reserved(std::string const in)
{
  for(auto it: reserved_words)
    if(in == it)
      return true;
  return false;
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

std::pair<std::string,int> get_word(const char* in, uint32_t size, uint32_t start, const char* end_set)
{
  uint32_t i=start;
  while(i<size && !is_in(in[i], end_set))
    i++;

  return std::make_pair(std::string(in+start, i-start), i);
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

// parse fcts

std::pair<subshell*, uint32_t> parse_subshell(const char* in, uint32_t size, uint32_t start);

// parse an arithmetic
// ends at ))
// for now, uses subshell parsing then takes raw string value
// temporary, to improve
std::pair<subarg_arithmetic*, uint32_t> parse_arithmetic(const char* in, uint32_t size, uint32_t start)
{
  subarg_arithmetic* ret = new subarg_arithmetic;
  uint32_t i=start;

  try
  {
    auto pp=parse_subshell(in, size, i);
    i=pp.second;
    delete pp.first;
    if(i >= size || in[i]!=')')
    {
      throw ztd::format_error( "Unexpected token ')', expecting '))'", g_origin, in, i );
    }
    ret->val = std::string(in+start, i-start-1);
    i++;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

// parse one argument
// must start at a read char
// ends at either " \t|&;\n()"
std::pair<arg*, uint32_t> parse_arg(const char* in, uint32_t size, uint32_t start)
{
  arg* ret = new arg;
  // j : start of subarg
  uint32_t i=start,j=start,q=start;

  try
  {

    if(is_in(in[i], SPECIAL_TOKENS))
      throw ztd::format_error( strf("Unexpected token '%c'", in[i]) , g_origin, in, i);

    while(i<size && !is_in(in[i], ARG_END))
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
            ret->sa.push_back(new subarg_string(std::string(in+j, i-j)));
            i+=3;
            // get arithmetic
            auto r=parse_arithmetic(in, size, i);
            ret->sa.push_back(r.first);
            j = i = r.second;
          }
          else if( word_eq("$(", in, size, i) ) // substitution
          {
            // add previous subarg
            ret->sa.push_back(new subarg_string(std::string(in+j, i-j)));
            i+=2;
            // get subshell
            auto r=parse_subshell(in, size, i);
            ret->sa.push_back(new subarg_subshell(r.first));
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
        ret->sa.push_back(new subarg_string(std::string(in+j, i-j)));
        i+=3;
        // get arithmetic
        auto r=parse_arithmetic(in, size, i);
        ret->sa.push_back(r.first);
        j = i = r.second;
      }
      else if( word_eq("$(", in, size, i) ) // substitution
      {
        // add previous subarg
        ret->sa.push_back(new subarg_string(std::string(in+j, i-j)));
        i+=2;
        // get subshell
        auto r=parse_subshell(in, size, i);
        ret->sa.push_back(new subarg_subshell(r.first));
        j = i = r.second;
      }
      else if( word_eq("$#", in, size, i) )
        i+=2;
      else
        i++;
    }

    // add string subarg
    std::string val=std::string(in+j, i-j);
    ret->sa.push_back(new subarg_string(val));

    // raw string for other uses
    ret->raw = std::string(in+start, i-start);

  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

// parse one list of arguments (a command for instance)
// must start at a read char
// first char has to be read
// ends at either &|;\n#()
std::pair<arglist*, uint32_t> parse_arglist(const char* in, uint32_t size, uint32_t start, bool hard_error=false)
{
  uint32_t i=start;
  arglist* ret = new arglist;

  try
  {
    if(is_in(in[i], SPECIAL_TOKENS))
    {
      if(hard_error)
        throw ztd::format_error( strf("Unexpected token '%c'", in[i]) , g_origin, in, i);
      else
        return std::make_pair(ret, i);
    }
    while(i<size)
    {
      auto pp=parse_arg(in, size, i);
      ret->args.push_back(pp.first);
      i = skip_chars(in, size, pp.second, SPACES);
      if(i>=size)
        return std::make_pair(ret, i);
      if(is_in(in[i], SPECIAL_TOKENS) )
        return std::make_pair(ret, i);
    }
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }
  return std::make_pair(ret, i);
}

std::pair<block*, uint32_t> parse_block(const char* in, uint32_t size, uint32_t start);

// parse a pipeline
// must start at a read char
// separated by |
// ends at either &;\n#)
std::pair<pipeline*, uint32_t> parse_pipeline(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  pipeline* ret = new pipeline;
  try
  {
    if(in[i] == '!' && i+1<size && is_in(in[i+1], SPACES))
    {
      ret->negated = true;
      i=skip_chars(in, size, i+1, SPACES);
    }
    while(i<size)
    {
      auto pp=parse_block(in, size, i);
      ret->add(pp.first);
      i = skip_chars(in, size, pp.second, SPACES);
      if( i>=size || is_in(in[i], PIPELINE_END) || word_eq("||", in, size, i) )
        return std::make_pair(ret, i);
      else if( in[i] != '|' )
        throw ztd::format_error( strf("Unexpected token: '%c'", in[i] ), g_origin, in, i);
      i++;
    }
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }
  return std::make_pair(ret, i);
}

// parse condition lists
// must start at a read char
// separated by && or ||
// ends at either ;\n)#
std::pair<condlist*, uint32_t> parse_condlist(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  condlist* ret = new condlist;
  try
  {
    bool optype=AND_OP;
    while(i<size)
    {
      auto pp=parse_pipeline(in, size, i);
      ret->add(pp.first, optype);
      i = pp.second;
      if(i>=size || is_in(in[i], CONTROL_END)) // end here exactly: used for control later
      {
        return std::make_pair(ret, i);
      }
      else if(is_in(in[i], COMMAND_SEPARATOR)) // end one char after: skip them for next parse
      {
        return std::make_pair(ret, i+1);
      }
      else if( word_eq("&", in, size, i) && !word_eq("&&", in, size, i) ) // parallel: end one char after
      {
        ret->parallel=true;
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
      else
        throw ztd::format_error( strf("Unexpected token: '%c'", in[i]), g_origin, in, i);
      i = skip_unread(in, size, i);
      if(i>=size)
        throw ztd::format_error( "Unexpected end of file", g_origin, in, i );
    }
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }
  return std::make_pair(ret, i);
}

std::pair<list_t, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, char end_c)
{
  std::vector<condlist*> ret;
  uint32_t i=skip_unread(in, size, start);
  try
  {
    while(end_c == 0 || in[i] != end_c)
    {
      auto pp=parse_condlist(in, size, i);
      ret.push_back(pp.first);
      i = skip_unread(in, size, pp.second);
      if(i>=size)
      {
        if(end_c != 0)
          throw ztd::format_error(strf("Expecting '%c'", end_c), g_origin, in, start-1);
        else
          break;
      }
    }
  }
  catch(ztd::format_error& e)
  {
    for(auto it: ret)
    delete it;
    throw e;
  }
  return std::make_pair(ret, i);
}

std::pair<list_t, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, std::string const& end_word)
{
  std::vector<condlist*> ret;
  uint32_t i=skip_unread(in, size, start);
  try
  {
    std::string old_expect=g_expecting;
    g_expecting=end_word;
    while(true)
    {
      // check word
      auto wp=get_word(in, size, i, ARG_END);
      if(wp.first == end_word)
      {
        i=wp.second;
        break;
      }
      // do a parse
      auto pp=parse_condlist(in, size, i);
      ret.push_back(pp.first);
      i = skip_unread(in, size, pp.second);
      // word wasn't found
      if(i>=size)
      {
        throw ztd::format_error(strf("Expecting '%s'", end_word.c_str()), g_origin, in, start-1);
      }
    }
    g_expecting=old_expect;
  }
  catch(ztd::format_error& e)
  {
    for(auto it: ret)
    delete it;
    throw e;
  }
  return std::make_pair(ret, i);
}


std::tuple<list_t, uint32_t, std::string> parse_list_until(const char* in, uint32_t size, uint32_t start, std::vector<std::string> const& end_words)
{
  std::vector<condlist*> ret;
  uint32_t i=skip_unread(in, size, start);;
  std::string found_end_word;
  try
  {
    std::string old_expect=g_expecting;
    g_expecting=end_words[0];
    bool stop=false;
    while(true)
    {
      // check words
      auto wp=get_word(in, size, i, ARG_END);
      for(auto it: end_words)
      {
        if(wp.first == it)
        {
          found_end_word=it;
          i=wp.second;
          stop=true;
          break;
        }
      }
      if(stop)
        break;
      // do a parse
      auto pp=parse_condlist(in, size, i);
      ret.push_back(pp.first);
      i = skip_unread(in, size, pp.second);
      // word wasn't found
      if(i>=size)
      {
        throw ztd::format_error(strf("Expecting '%s'", end_words[0].c_str()), g_origin, in, start-1);
      }
    }
    g_expecting=old_expect;
  }
  catch(ztd::format_error& e)
  {
    for(auto it: ret)
    delete it;
    throw e;
  }
  return std::make_tuple(ret, i, found_end_word);
}

// parse a subshell
// must start right after the opening (
// ends at ) and nothing else
std::pair<subshell*, uint32_t> parse_subshell(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  subshell* ret = new subshell;

  try
  {
    auto pp=parse_list_until(in, size, start, ')');
    ret->cls=pp.first;
    i=pp.second;
    if(ret->cls.size()<=0)
      throw ztd::format_error("Subshell is empty", g_origin, in, start-1);
    i++;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret,i);
}


// parse a brace block
// must start right after the opening {
// ends at } and nothing else
std::pair<brace*, uint32_t> parse_brace(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_unread(in, size, start);
  brace* ret = new brace;

  try
  {
    auto pp=parse_list_until(in, size, start, '}');
    ret->cls=pp.first;
    i=pp.second;
    if(ret->cls.size()<=0)
      throw ztd::format_error("Brace block is empty", g_origin, in, start-1);
    i++;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret,i);
}

// parse a function
// must start right after the ()
// then parses a brace block
std::pair<function*, uint32_t> parse_function(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  function* ret = new function;

  try
  {
    i=skip_unread(in, size, i);
    if(in[i] != '{')
      throw ztd::format_error("Expecting { after ()", g_origin, in, i);
    i++;

    auto pp=parse_list_until(in, size, i, '}');
    if(pp.first.size()<=0)
      throw ztd::format_error("Condition is empty", g_origin, in, i);

    ret->cls=pp.first;
    i=pp.second;
    i++;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<cmd*, uint32_t> parse_cmd(const char* in, uint32_t size, uint32_t start)
{
  cmd* ret = new cmd;
  uint32_t i=start;

  try
  {
    auto pp=parse_arglist(in, size, start, true);
    ret->args = pp.first;
    i = pp.second;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

// parse a case block
// must start right after the case
// ends at } and nothing else
std::pair<case_block*, uint32_t> parse_case(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=skip_chars(in, size, start, SPACES);;
  case_block* ret = new case_block;

  try
  {
    // get the treated argument
    auto pa = parse_arg(in, size, i);
    ret->carg = pa.first;
    i=skip_unread(in, size, pa.second);

    // must be an 'in'
    if(!word_eq("in", in, size, i, SEPARATORS))
    {
      std::string pp=get_word(in, size, i, SEPARATORS).first;
      throw ztd::format_error("Unexpected word: '"+pp+"', expecting 'in' after case", g_origin, in, i);
    }

    i=skip_unread(in, size, i+2);

    // parse all cases
    while(i<size && !word_eq("esac", in, size, i, ARG_END) )
    {
      // add one element
      ret->cases.push_back( std::pair<arglist_t, list_t>() );
      // iterator to last element
      auto cc = ret->cases.end()-1;

      // toto)
      while(true)
      {
        pa = parse_arg(in, size, i);
        cc->first.push_back(pa.first);
        if(pa.first->raw == "")
          throw ztd::format_error("Empty case value", g_origin, in, i);
        i=skip_unread(in, size, pa.second);
        if(i>=size)
          throw ztd::format_error("Unexpected end of file. Expecting 'esac'", g_origin, in, i);
        if(in[i] == ')')
          break;
        if(in[i] != '|' && is_in(in[i], SPECIAL_TOKENS))
          throw ztd::format_error( strf("Unexpected token '%c', expecting ')'", in[i]), g_origin, in, i );
        i=skip_unread(in, size, i+1);
      }
      i++;

      while(true) // blocks
      {
        auto pc = parse_condlist(in, size, i);
        cc->second.push_back(pc.first);
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
    }

    // ended before finding esac
    if(i>=size)
      throw ztd::format_error("Expecting 'esac'", g_origin, in, i);
    i+=4;
  }
  catch(ztd::format_error& e)
  {
    if(ret != nullptr) delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<if_block*, uint32_t> parse_if(const char* in, uint32_t size, uint32_t start)
{
  if_block* ret = new if_block;
  uint32_t i=start;

  try
  {
    while(true)
    {
      std::pair<list_t,list_t> ll;
      std::string word;

      try
      {
        auto pp=parse_list_until(in, size, i, "then");
        if(pp.first.size()<=0)
          throw ztd::format_error("Condition is empty", g_origin, in, i);
        i=pp.second;
        ll.first=pp.first;

        auto tp=parse_list_until(in, size, i, {"fi", "elif", "else"});
        if(std::get<0>(tp).size() <= 0)
          throw ztd::format_error("if block is empty", g_origin, in, i);
        ll.second = std::get<0>(tp);
        i=std::get<1>(tp);
        word=std::get<2>(tp);

        ret->blocks.push_back(ll);
      }
      catch(ztd::format_error& e)
      {
        for(auto it: ll.first)
          delete it;
        for(auto it: ll.second)
          delete it;
        throw e;
      }

      if(word == "fi")
        break;
      if(word == "else")
      {
        auto pp=parse_list_until(in, size, i, "fi");
        if(pp.first.size()<=0)
          throw ztd::format_error("else block is empty", g_origin, in, i);
        ret->else_cls=pp.first;
        i=pp.second;
        break;
      }

    }

  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<for_block*, uint32_t> parse_for(const char* in, uint32_t size, uint32_t start)
{
  for_block* ret = new for_block;
  uint32_t i=skip_chars(in, size, start, SPACES);

  try
  {
    auto wp = get_word(in, size, i, ARG_END);

    if(!valid_name(wp.first))
      throw ztd::format_error( strf("Bad identifier in for clause: '%s'", wp.first.c_str()), g_origin, in, i );
    ret->varname = wp.first;
    i=skip_chars(in, size, wp.second, SPACES);

    // in
    wp = get_word(in, size, i, ARG_END);
    if(wp.first == "in")
    {
      i=skip_chars(in, size, wp.second, SPACES);
      auto pp = parse_arglist(in, size, i);
      ret->iter = pp.first;
      i = pp.second;
    }
    else if(wp.first != "")
      throw ztd::format_error( "Expecting 'in' after for", g_origin, in, i );

    // end of arg list
    if(!is_in(in[i], "\n;#"))
      throw ztd::format_error( strf("Unexpected token '%c', expecting '\\n' or ';'", in[i]), g_origin, in, i );
    if(in[i] == ';')
      i++;
    i=skip_unread(in, size, i);

    // do
    wp = get_word(in, size, i, ARG_END);
    if(wp.first != "do")
      throw ztd::format_error( "Expecting 'do', after for", g_origin, in, i);
    i=skip_unread(in, size, wp.second);

    // ops
    auto lp = parse_list_until(in, size, i, "done");
    ret->ops=lp.first;
    i=lp.second;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<while_block*, uint32_t> parse_while(const char* in, uint32_t size, uint32_t start)
{
  while_block* ret = new while_block;
  uint32_t i=start;

  try
  {
    // cond
    auto pp=parse_list_until(in, size, i, "do");
    if(pp.first.size() <= 0)
      throw ztd::format_error("condition is empty", g_origin, in, i);

    ret->cond = pp.first;
    i=pp.second;

    // ops
    auto lp = parse_list_until(in, size, i, "done");
    if(lp.first.size() <= 0)
      throw ztd::format_error("while is empty", g_origin, in, i);
    ret->ops=lp.first;
    i=lp.second;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<block*, uint32_t> parse_fct_or_cmd(const char* in, uint32_t size, uint32_t start)
{
  block* ret = nullptr;
  uint32_t i=start;

  try
  {
    // get first word
    auto tp=get_word(in, size, start, ARG_END);

    i=skip_unread(in, size, tp.second);
    if(word_eq("()", in, size, i)) // is a function
    {
      if(!valid_name(tp.first))
        throw ztd::format_error( strf("Bad function name: '%s'", tp.first.c_str()), g_origin, in, start );

      auto pp = parse_function(in, size, i+2);
      // first arg is function name
      pp.first->name = tp.first;
      ret = pp.first;
      i = pp.second;
    }
    else // is a command
    {
      auto pp = parse_cmd(in, size, start);
      ret = pp.first;
      i = pp.second;
    }
  }
  catch(ztd::format_error& e)
  {
    if(ret!=nullptr) delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

// detect if brace, subshell, case or other
std::pair<block*, uint32_t> parse_block(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i = skip_chars(in, size, start, SEPARATORS);
  block* ret = nullptr;

  try
  {
    if(i>=size)
      throw ztd::format_error("Unexpected end of file", g_origin, in, i);
    if( in[i] == '(' ) //subshell
    {
      auto pp = parse_subshell(in, size, i+1);
      ret = pp.first;
      i = pp.second;
    }
    else
    {
      auto wp=get_word(in, size, i, SEPARATORS);
      std::string word=wp.first;
      if( word == "{" ) // brace block
      {
        auto pp = parse_brace(in, size, wp.second);
        ret = pp.first;
        i = pp.second;
      }
      else if(word == "case") // case
      {
        auto pp = parse_case(in, size, wp.second);
        ret = pp.first;
        i = pp.second;
      }
      else if( word == "if" ) // if
      {
        auto pp=parse_if(in, size, wp.second);
        ret = pp.first;
        i = pp.second;
      }
      else if( word == "for" )
      {
        auto pp=parse_for(in, size, wp.second);
        ret = pp.first;
        i = pp.second;
      }
      else if( word == "while" )
      {
        auto pp=parse_while(in, size, wp.second);
        ret = pp.first;
        i = pp.second;
      }
      else if( word == "until")
      {
        auto pp=parse_while(in, size, wp.second);
        pp.first->real_condition()->negate();
        ret = pp.first;
        i = pp.second;
      }
      else if(word_is_reserved(word))
      {
        throw ztd::format_error( "Unexpected '"+word+"'" + expecting(g_expecting) , g_origin, in, i);
      }
      else // other: command/function
      {
        auto pp = parse_fct_or_cmd(in, size, i);
        ret = pp.first;
        i = pp.second;
      }

    }

    if(ret->type != block::block_cmd)
    {
      auto pp=parse_arglist(in, size, i, false); // in case of redirects
      if(pp.first->args.size()>0)
      {
        i = pp.second;
        ret->redirs=pp.first;
      }
      else
      {
        delete pp.first;
      }
    }
  }
  catch(ztd::format_error& e)
  {
    if(ret != nullptr) delete ret;
    throw e;
  }

  return std::make_pair(ret,i);
}

// parse main
shmain* parse(const char* in, uint32_t size)
{
  shmain* ret = new shmain();
  uint32_t i=0;
  try
  {
    // get shebang
    if(word_eq("#!", in, size, 0))
    {
      i=skip_until(in, size, 0, "\n");
      ret->shebang=std::string(in, i);
    }
    i = skip_unread(in, size, i);
    // parse all commands
    auto pp=parse_list_until(in, size, i, 0);
    ret->cls=pp.first;
    i=pp.second;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
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
