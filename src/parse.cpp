#include "parse.hpp"

#include <fstream>
#include <strings.h>
#include <string.h>

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"

#define ORIGIN_NONE ""

bool g_bash=false;

// macro

#define PARSE_ERROR(str, i) ztd::format_error(str, "", in, i)

// constants

const std::vector<std::string> all_reserved_words = { "if", "then", "else", "fi", "case", "esac", "for", "while", "do", "done", "{", "}" };
const std::vector<std::string> out_reserved_words = { "then", "else", "fi", "esac", "do", "done", "}" };

// stuff

std::string g_expecting;

std::string expecting(std::string const& word)
{
  if(word != "")
    return ", expecting '"+word+"'";
  else
    return "";
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

bool word_is_reserved_out(std::string const in)
{
  for(auto it: out_reserved_words)
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

std::pair<std::string,uint32_t> get_word(const char* in, uint32_t size, uint32_t start, const char* end_set)
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

std::pair<std::string, uint32_t> parse_varname(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;
  std::string ret;

  // special vars
  if(is_in(in[i], SPECIAL_VARS) || (in[i]>='0' && in[i]<='1'))
  {
    ret=in[i];
    i++;
  }
  else // varname
  {
    while(i<size && (is_alphanum(in[i]) || in[i] == '_') )
      i++;
    ret = std::string(in+start, i-start);
  }

  return std::make_pair(ret, i);
}

// parse an arithmetic
// ends at ))
// for now, uses subshell parsing then takes raw string value
// temporary, to improve
std::pair<arithmetic_subarg*, uint32_t> parse_arithmetic(const char* in, uint32_t size, uint32_t start)
{
  arithmetic_subarg* ret = new arithmetic_subarg;
  uint32_t i=start;

  try
  {
    auto pp=parse_subshell(in, size, i);
    i=pp.second;
    delete pp.first;
    if(i >= size || in[i]!=')')
    {
      throw PARSE_ERROR( "Unexpected token ')', expecting '))'", i );
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

std::pair<arg*, uint32_t> parse_arg(const char* in, uint32_t size, uint32_t start, const char* end=ARG_END, const char* unexpected=SPECIAL_TOKENS, bool doquote=true);

std::pair<manipulation_subarg*, uint32_t> parse_manipulation(const char* in, uint32_t size, uint32_t start)
{
  manipulation_subarg* ret = new manipulation_subarg;
  uint32_t i=start;

  if(in[i] == '#')
  {
    ret->size=true;
    i++;
  }

  auto p=parse_varname(in, size, i);
  if(p.second == i)
    throw PARSE_ERROR( "Bad variable name", i );
  ret->varname=p.first;
  i = p.second;

  auto pa = parse_arg(in, size, i, "}", NULL, false);
  ret->manip=pa.first;
  i = pa.second+1;

  return std::make_pair(ret, i);
}

// parse one argument
// must start at a read char
// ends at either " \t|&;\n()"
std::pair<arg*, uint32_t> parse_arg(const char* in, uint32_t size, uint32_t start, const char* end, const char* unexpected, bool doquote)
{
  arg* ret = new arg;
  // j : start of subarg , q = start of quote
  uint32_t i=start,j=start,q=start;

  try
  {

    if(unexpected != NULL && is_in(in[i], unexpected))
      throw PARSE_ERROR( strf("Unexpected token '%c'", in[i]) , i);

    while(i<size && !(end != NULL && is_in(in[i], end)) )
    {
      if(i+1<size && is_in(in[i], "<>") && in[i+1]=='&') // special case for <& and >&
      {
        i+=2;
      }
      else if(doquote && in[i]=='\\') // backslash: don't check next char
      {
        i++;
        if(i>=size)
          break;
        i++;
      }
      else if(doquote && in[i] == '"') // start double quote
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
            std::string tmpstr=std::string(in+j, i-j);
            if(tmpstr!="")
              ret->sa.push_back(new string_subarg(tmpstr));
            // get arithmetic
            auto r=parse_arithmetic(in, size, i+3);
            ret->sa.push_back(r.first);
            j = i = r.second;
          }
          else if( word_eq("$(", in, size, i) ) // substitution
          {
            // add previous subarg
            std::string tmpstr=std::string(in+j, i-j);
            if(tmpstr!="")
              ret->sa.push_back(new string_subarg(tmpstr));
            // get subshell
            auto r=parse_subshell(in, size, i+2);
            ret->sa.push_back(new subshell_subarg(r.first, true));
            j = i = r.second;
          }
          else if( word_eq("${", in, size, i) ) // variable manipulation
          {
            // add previous subarg
            std::string tmpstr=std::string(in+j, i-j);
            if(tmpstr!="")
              ret->sa.push_back(new string_subarg(tmpstr));
            // get manipulation
            auto r=parse_manipulation(in, size, i+2);
            ret->sa.push_back(r.first);
            j = i = r.second;
          }
          else if( in[i] == '$' )
          {
            auto r=parse_varname(in, size, i+1);
            if(r.second > i+1)
            {
              // add previous subarg
              std::string tmpstr=std::string(in+j, i-j);
              if(tmpstr!="")
                ret->sa.push_back(new string_subarg(tmpstr));
              // add varname
              ret->sa.push_back(new variable_subarg(r.first));
              j = i = r.second;
            }
            else
              i++;
          }
          else
            i++;

          if(i>=size)
            throw PARSE_ERROR("Unterminated double quote", q);
        }
        i++;
      }
      else if(doquote && in[i] == '\'') // start single quote
      {
        q=i;
        i++;
        while(i<size && in[i]!='\'')
          i++;
        if(i>=size)
          throw PARSE_ERROR("Unterminated single quote", q);
        i++;
      }
      else if( word_eq("$((", in, size, i) ) // arithmetic operation
      {
        // add previous subarg
        std::string tmpstr=std::string(in+j, i-j);
        if(tmpstr!="")
          ret->sa.push_back(new string_subarg(tmpstr));
        // get arithmetic
        auto r=parse_arithmetic(in, size, i+3);
        ret->sa.push_back(r.first);
        j = i = r.second;
      }
      else if( word_eq("$(", in, size, i) ) // substitution
      {
        // add previous subarg
        std::string tmpstr=std::string(in+j, i-j);
        if(tmpstr!="")
          ret->sa.push_back(new string_subarg(tmpstr));
        // get subshell
        auto r=parse_subshell(in, size, i+2);
        ret->sa.push_back(new subshell_subarg(r.first, false));
        j = i = r.second;
      }
      else if( word_eq("${", in, size, i) ) // variable manipulation
      {
        // add previous subarg
        std::string tmpstr=std::string(in+j, i-j);
        if(tmpstr!="")
          ret->sa.push_back(new string_subarg(tmpstr));
        // get manipulation
        auto r=parse_manipulation(in, size, i+2);
        ret->sa.push_back(r.first);
        j = i = r.second;
      }
      else if( in[i] == '$' )
      {
        auto r=parse_varname(in, size, i+1);
        if(r.second > i+1)
        {
          // add previous subarg
          std::string tmpstr=std::string(in+j, i-j);
          if(tmpstr!="")
            ret->sa.push_back(new string_subarg(tmpstr));
          // add varname
          ret->sa.push_back(new variable_subarg(r.first));
          j = i = r.second;
        }
        else
          i++;
      }
      else
        i++;
    }

    // add string subarg
    std::string val=std::string(in+j, i-j);
    if(val != "")
      ret->sa.push_back(new string_subarg(val));

  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }

  return std::make_pair(ret, i);
}

std::pair<redirect*, uint32_t> parse_redirect(const char* in, uint32_t size, uint32_t start)
{
  uint32_t i=start;

  bool is_redirect=false;
  bool needs_arg=false;
  bool has_num_prefix=false;

  if(is_num(in[i]))
  {
    i++;
    has_num_prefix=true;
  }

  if( in[i] == '>' )
  {
    i++;
    if(i>size)
      throw PARSE_ERROR("Unexpected end of file", i);
    is_redirect = true;
    if(i+1<size && in[i] == '&' && is_num(in[i+1]) )
    {
      i+=2;
      needs_arg=false;
    }
    else if(in[i] == '&') // >& bash operator
    {
      if(!g_bash)
        throw PARSE_ERROR("bash specific: '>&'. Use --debashify to remove bashisms", i);
      i++;
      needs_arg=true;
    }
    else
    {
      if(in[i] == '>')
        i++;
      needs_arg=true;
    }
  }
  else if( in[i] == '<' )
  {
    if(has_num_prefix)
      throw PARSE_ERROR("Invalid input redirection", i-1);
    i++;
    if(i>size)
      throw PARSE_ERROR("Unexpected end of file", i);
    if(in[i] == '<')
    {
      i++;
      if(i<size && in[i] == '<')
      {
        if(!g_bash)
          throw PARSE_ERROR("bash specific: '<<<'. Use --debashify to remove bashisms", i);
        i++;
      }
    }
    is_redirect=true;
    needs_arg=true;
  }
  else if( word_eq("&>", in, size, i) ) // &> bash operator
  {
    if(!g_bash)
      throw PARSE_ERROR("bash specific: '&>'. Use --debashify to remove bashisms", i);
    i+=2;
    is_redirect=true;
    needs_arg=true;
  }


  if(is_redirect)
  {
    redirect* ret=nullptr;
    try
    {
      ret = new redirect;
      ret->op = std::string(in+start, i-start);
      if(needs_arg)
      {
        i = skip_chars(in, size, i, SPACES);
        if(ret->op == "<<")
        {
          auto pa = parse_arg(in, size, i);
          std::string delimitor = pa.first->string();
          delete pa.first;
          pa.first = nullptr;

          if(delimitor == "")
            throw PARSE_ERROR("Non-static or empty text input delimitor", i);

          if(delimitor.find('"') != std::string::npos || delimitor.find('\'') != std::string::npos || delimitor.find('\\') != std::string::npos)
          {
            delimitor = ztd::sh("echo "+delimitor); // shell resolve the delimitor
            delimitor.pop_back(); // remove \n
          }

          i = skip_chars(in, size, pa.second, SPACES);  // skip spaces

          if(in[i] == '#') // skip comment
            i = skip_until(in, size, i, "\n"); //skip to endline
          if(in[i] != '\n') // has another arg
            throw PARSE_ERROR("Additionnal argument after text input delimitor", i);

          i++;
          uint32_t j=i;
          char* tc=NULL;
          tc = (char*) strstr(in+i, std::string("\n"+delimitor+"\n").c_str()); // find delimitor
          if(tc!=NULL) // delimitor was found
          {
            i = (tc-in)+delimitor.size()+1;
          }
          else
          {
            i = size;
            // maybe at end of file with no \n
            // if(strstr(in+size-delimitor.size(), std::string("\n"+delimitor).c_str())!=NULL)
            //   i = size-delimitor.size();
            // else // not found: end of file
          }
          std::string tmpparse=std::string(in+j, i-j);
          auto pval = parse_arg(tmpparse.c_str(), tmpparse.size(), 0, NULL);
          ret->target = pval.first;
          ret->target->sa.insert(ret->target->sa.begin(), new string_subarg(delimitor+"\n"));
        }
        else
        {
          auto pa = parse_arg(in, size, i);
          ret->target = pa.first;
          i=pa.second;
        }
      }
    }
    catch(ztd::format_error& e)
    {
      if(ret!=nullptr)
        delete ret;
      throw e;
    }
    return std::make_pair(ret, i);
  }
  else
    return std::make_pair(nullptr, start);
}

// parse one list of arguments (a command for instance)
// must start at a read char
// first char has to be read
// ends at either &|;\n#()
std::pair<arglist*, uint32_t> parse_arglist(const char* in, uint32_t size, uint32_t start, bool hard_error=false, std::vector<redirect*>* redirs=nullptr)
{
  uint32_t i=start;
  arglist* ret = nullptr;

  try
  {
    if(is_in(in[i], SPECIAL_TOKENS) && !word_eq("&>", in, size, i))
    {
      if(hard_error)
        throw PARSE_ERROR( strf("Unexpected token '%c'", in[i]) , i);
      else
        return std::make_pair(ret, i);
    }
    while(i<size)
    {
      if(redirs!=nullptr)
      {
        auto pr = parse_redirect(in, size, i);
        if(pr.first != nullptr)
        {
          redirs->push_back(pr.first);
          i=pr.second;
        }
        else
          goto argparse;
      }
      else
      {
argparse:
        if(ret == nullptr)
          ret = new arglist;
        auto pp=parse_arg(in, size, i);
        ret->args.push_back(pp.first);
        i = pp.second;
      }
      i = skip_chars(in, size, i, SPACES);
      if(word_eq("&>", in, size, i))
        // throw PARSE_ERROR("Unsupported &>", i);
        continue;
      if(word_eq("|&", in, size, i))
        throw PARSE_ERROR("Unsupported |&, use '2>&1 |' instead", i);
      if(i>=size)
        return std::make_pair(ret, i);
      if( is_in(in[i], SPECIAL_TOKENS) )
        return std::make_pair(ret, i);
    }
  }
  catch(ztd::format_error& e)
  {
    if(ret != nullptr)
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
        throw PARSE_ERROR( strf("Unexpected token: '%c'", in[i] ), i);
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
      if(i>=size || is_in(in[i], CONTROL_END) || is_in(in[i], COMMAND_SEPARATOR)) // end here exactly: used for control later
      {
        return std::make_pair(ret, i);
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
        throw PARSE_ERROR( strf("Unexpected token: '%c'", in[i]), i);
      i = skip_unread(in, size, i);
      if(i>=size)
        throw PARSE_ERROR( "Unexpected end of file", i );
    }
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }
  return std::make_pair(ret, i);
}

std::pair<list*, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, char end_c, const char* expecting=NULL)
{
  list* ret = new list;
  uint32_t i=skip_unread(in, size, start);
  try
  {
    while(in[i] != end_c)
    {
      auto pp=parse_condlist(in, size, i);
      ret->cls.push_back(pp.first);
      i=pp.second;

      if(i < size)
      {
        if(in[i] == end_c) // end char, stop here
          break;
        else if(in[i] == '#')
          ; // skip here
        else if(is_in(in[i], COMMAND_SEPARATOR))
          i++; // skip on next char
        else if(is_in(in[i], CONTROL_END))
          throw PARSE_ERROR(strf("Unexpected token: '%c'", in[i]), i);

        i = skip_unread(in, size, i);
      }

      if(i>=size)
      {
        if(end_c != 0)
        {
          if(expecting!=NULL)
            throw PARSE_ERROR(strf("Expecting '%s'", expecting), start-1);
          else
            throw PARSE_ERROR(strf("Expecting '%c'", end_c), start-1);
        }
        else
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

std::pair<list*, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, std::string const& end_word)
{
  list* ret = new list;
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
      ret->cls.push_back(pp.first);
      i=pp.second;
      if(i<size)
      {
        if(in[i] == '#')
          ; // skip here
        else if(is_in(in[i], COMMAND_SEPARATOR))
          i++;
        else if(is_in(in[i], CONTROL_END))
          throw PARSE_ERROR(strf("Unexpected token: '%c'", in[i]), i);

        i = skip_unread(in, size, i);
      }

      // word wasn't found
      if(i>=size)
      {
        throw PARSE_ERROR(strf("Expecting '%s'", end_word.c_str()), start-1);
      }
    }
    g_expecting=old_expect;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw e;
  }
  return std::make_pair(ret, i);
}


std::tuple<list*, uint32_t, std::string> parse_list_until(const char* in, uint32_t size, uint32_t start, std::vector<std::string> const& end_words, const char* expecting=NULL)
{
  list* ret = new list;
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
        if(it == ";" && in[i] == ';')
        {
          found_end_word=";";
          i++;
          stop=true;
          break;
        }
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
      ret->cls.push_back(pp.first);
      i=pp.second;
      if(in[i] == '#')
        ; // skip here
      else if(is_in(in[i], COMMAND_SEPARATOR))
        i++; // skip on next

      i = skip_unread(in, size, i);
      // word wasn't found
      if(i>=size)
      {
        if(expecting!=NULL)
          throw PARSE_ERROR(strf("Expecting '%s'", expecting), start-1);
        else
          throw PARSE_ERROR(strf("Expecting '%s'", end_words[0].c_str()), start-1);
      }
    }
    g_expecting=old_expect;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
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
    ret->lst=pp.first;
    i=pp.second;
    if(ret->lst->size()<=0)
      throw PARSE_ERROR("Subshell is empty", start-1);
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
    ret->lst=pp.first;
    i=pp.second;
    if(ret->lst->size()<=0)
      throw PARSE_ERROR("Brace block is empty", start-1);
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
std::pair<function*, uint32_t> parse_function(const char* in, uint32_t size, uint32_t start, const char* after="()")
{
  uint32_t i=start;
  function* ret = new function;

  try
  {
    i=skip_unread(in, size, i);
    if(in[i] != '{')
      throw PARSE_ERROR( strf("Expecting { after %s", after) , i);
    i++;

    auto pp=parse_list_until(in, size, i, '}');
    if(pp.first->size()<=0)
      throw PARSE_ERROR("Function is empty", i);

    ret->lst=pp.first;
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

// must start at read char
std::pair<cmd*, uint32_t> parse_cmd(const char* in, uint32_t size, uint32_t start)
{
  cmd* ret = new cmd;
  uint32_t i=start;

  try
  {
    while(true) // parse var assigns
    {
      auto wp=get_word(in, size, i, VARNAME_END);
      if(wp.second<size && in[wp.second] == '=' && valid_name(wp.first)) // is a var assign
      {
        i=wp.second+1;
        arg* ta;
        if( is_in(in[i], ARG_END) ) // no value : give empty value
        {
          ta = new arg;
        }
        else
        {
          auto pp=parse_arg(in, size, i);
          ta=pp.first;
          i=pp.second;
        }
        ret->var_assigns.push_back(std::make_pair(wp.first, ta));
        i=skip_chars(in, size, i, " \t");
      }
      else
        break;
    }

    if(!is_in(in[i], SPECIAL_TOKENS))
    {
      auto pp=parse_arglist(in, size, i, true, &ret->redirs);
      ret->args = pp.first;
      i = pp.second;
    }
    else if(ret->var_assigns.size() <= 0)
      throw PARSE_ERROR( strf("Unexpected token: '%c'", in[i]), i );

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
      throw PARSE_ERROR( strf("Unexpected word: '%s', expecting 'in' after case", pp.c_str()), i);
    }

    i=skip_unread(in, size, i+2);

    // parse all cases
    while(i<size && !word_eq("esac", in, size, i, ARG_END) )
    {
      // add one element
      ret->cases.push_back( std::make_pair(std::vector<arg*>(), nullptr) );
      // iterator to last element
      auto cc = ret->cases.end()-1;

      // toto)
      while(true)
      {
        pa = parse_arg(in, size, i);
        cc->first.push_back(pa.first);
        if(pa.first->sa.size() <= 0)
          throw PARSE_ERROR("Empty case value", i);
        i=skip_unread(in, size, pa.second);
        if(i>=size)
          throw PARSE_ERROR("Unexpected end of file. Expecting 'esac'", i);
        if(in[i] == ')')
          break;
        if(in[i] != '|' && is_in(in[i], SPECIAL_TOKENS))
          throw PARSE_ERROR( strf("Unexpected token '%c', expecting ')'", in[i]), i );
        i=skip_unread(in, size, i+1);
      }
      i++;

      // until ;;
      auto tp = parse_list_until(in, size, i, {";", "esac"}, ";;");
      cc->second = std::get<0>(tp);
      i = std::get<1>(tp);
      std::string word = std::get<2>(tp);
      if(word == "esac")
      {
        i -= 4;
        break;
      }
      if(i >= size)
        throw PARSE_ERROR("Expecting ';;'", i);
      if(in[i-1] != ';')
        throw PARSE_ERROR("Unexpected token: ';'", i);

      i=skip_unread(in, size, i+1);
    }

    // ended before finding esac
    if(i>=size)
      throw PARSE_ERROR("Expecting 'esac'", i);
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
      std::string word;

      ret->blocks.push_back(std::make_pair(nullptr, nullptr));
      auto ll = ret->blocks.end()-1;

      auto pp=parse_list_until(in, size, i, "then");
      ll->first = pp.first;
      i = pp.second;
      if(ll->first->size()<=0)
        throw PARSE_ERROR("Condition is empty", i);

      auto tp=parse_list_until(in, size, i, {"fi", "elif", "else"});
      ll->second = std::get<0>(tp);
      i = std::get<1>(tp);
      word = std::get<2>(tp);
      if(std::get<0>(tp)->size() <= 0)
        throw PARSE_ERROR("if block is empty", i);

      if(word == "fi")
        break;
      if(word == "else")
      {
        auto pp=parse_list_until(in, size, i, "fi");
        if(pp.first->size()<=0)
          throw PARSE_ERROR("else block is empty", i);
        ret->else_lst=pp.first;
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
      throw PARSE_ERROR( strf("Bad identifier in for clause: '%s'", wp.first.c_str()), i );
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
      throw PARSE_ERROR( "Expecting 'in' after for", i );

    // end of arg list
    if(!is_in(in[i], "\n;#"))
      throw PARSE_ERROR( strf("Unexpected token '%c', expecting '\\n' or ';'", in[i]), i );
    if(in[i] == ';')
      i++;
    i=skip_unread(in, size, i);

    // do
    wp = get_word(in, size, i, ARG_END);
    if(wp.first != "do")
      throw PARSE_ERROR( "Expecting 'do', after for", i);
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
    ret->cond = pp.first;
    i = pp.second;
    if(ret->cond->size() <= 0)
      throw PARSE_ERROR("condition is empty", i);


    // ops
    auto lp = parse_list_until(in, size, i, "done");
    ret->ops=lp.first;
    i = lp.second;
    if(ret->ops->size() <= 0)
      throw PARSE_ERROR("while is empty", i);
  }
  catch(ztd::format_error& e)
  {
    delete ret;
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
      throw PARSE_ERROR("Unexpected end of file", i);
    if( in[i] == '(' ) //subshell
    {
      auto pp = parse_subshell(in, size, i+1);
      ret = pp.first;
      i = pp.second;
    }
    else
    {
      auto wp=get_word(in, size, i, BLOCK_TOKEN_END);
      std::string word=wp.first;
      // reserved words
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
      else if( word == "until" )
      {
        auto pp=parse_while(in, size, wp.second);
        pp.first->real_condition()->negate();
        ret = pp.first;
        i = pp.second;
      }
      else if(word_is_reserved_out(word))
      {
        throw PARSE_ERROR( "Unexpected '"+word+"'" + expecting(g_expecting) , i);
      }
      // end reserved words
      else if( word == "function" ) // bash style function
      {
        if(!g_bash)
          throw PARSE_ERROR("bash specific: 'function'. Use --debashify to remove bashisms", i);
        auto wp2=get_word(in, size, skip_unread(in, size, wp.second), VARNAME_END);
        if(!valid_name(wp2.first))
          throw PARSE_ERROR( strf("Bad function name: '%s'", word.c_str()), start );

        i=skip_unread(in, size, wp2.second);
        if(word_eq("()", in, size, i))
          i=skip_unread(in, size, i+2);

        auto pp = parse_function(in, size, i, "function definition");
        // function name
        pp.first->name = wp2.first;
        ret = pp.first;
        i = pp.second;
      }
      else if(word_eq("()", in, size, skip_unread(in, size, wp.second))) // is a function
      {
        if(!valid_name(word))
          throw PARSE_ERROR( strf("Bad function name: '%s'", word.c_str()), start );

        auto pp = parse_function(in, size, skip_unread(in, size, wp.second)+2);
        // first arg is function name
        pp.first->name = word;
        ret = pp.first;
        i = pp.second;
      }
      else // is a command
      {
        auto pp = parse_cmd(in, size, i);
        ret = pp.first;
        i = pp.second;
      }

    }

    if(ret->type != block::block_cmd)
    {
      uint32_t j=skip_chars(in, size, i, SPACES);
      auto pp=parse_arglist(in, size, j, false, &ret->redirs); // in case of redirects
      if(pp.first != nullptr)
      {
        delete pp.first;
        throw PARSE_ERROR("Extra argument after block", i);
      }
      i=pp.second;
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
shmain* parse_text(const char* in, uint32_t size, std::string const& filename)
{
  shmain* ret = new shmain();
  uint32_t i=0;
  try
  {
    ret->filename=filename;
    // get shebang
    if(word_eq("#!", in, size, 0))
    {
      i=skip_until(in, size, 0, "\n");
      ret->shebang=std::string(in, i);
    }
    i = skip_unread(in, size, i);
    // do bash reading
    std::string binshebang = basename(ret->shebang);
    g_bash = binshebang == "bash" || binshebang == "lxsh" || options["debashify"];
    // parse all commands
    auto pp=parse_list_until(in, size, i, 0);
    ret->lst=pp.first;
    i=pp.second;
  }
  catch(ztd::format_error& e)
  {
    delete ret;
    throw ztd::format_error(e.what(), filename, e.data(), e.where());
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
