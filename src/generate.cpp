#include "struc.hpp"

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"
#include "parse.hpp"

std::vector<std::string> included;

bool is_sub_special_cmd(std::string in)
{
  return in == "%include_sub" || in == "%resolve_sub";
}

std::string indented(std::string const& in, uint32_t ind)
{
  if(!opt_minimize)
    return indent(ind) + in;
  else
    return in;
}

std::string arg::generate(int ind)
{
  std::string ret;
  for(auto it: sa)
  {
    ret += it->generate(ind);
  }
  return ret;
}

std::string arglist::generate(int ind)
{
  std::string ret;

  for(auto it: args)
  {
    ret += it->generate(ind);
    ret += ' ';
  }

  ret.pop_back();

  return ret;
}

std::string pipeline::generate(int ind)
{
  std::string ret;

  if(cmds.size()<=0)
    return "";

  if(negated)
    ret += "! ";
  ret += cmds[0]->generate(ind);
  for(uint32_t i=1 ; i<cmds.size() ; i++)
  {
    ret += opt_minimize ? "|" : " | " ;
    ret += cmds[i]->generate(ind);
  }

  return ret;
}

std::string condlist::generate(int ind)
{
  std::string ret;
  if(pls.size() <= 0)
    return "";
  ret += pls[0]->generate(ind);
  for(uint32_t i=0 ; i<pls.size()-1 ; i++)
  {
    if(or_ops[i])
      ret += opt_minimize ? "||" : " || ";
    else
      ret += opt_minimize ? "&&" : " && ";
    ret += pls[i+1]->generate(ind);
  }
  if(ret=="")
    return "";
  if(parallel)
  {
    ret += opt_minimize ? "&" : " &\n";
  }
  else
    ret += '\n';
  return ret;
}

std::string list::generate(int ind, bool first_indent)
{
  std::string ret;
  if(cls.size() <= 0)
    return "";

  for(uint32_t i=0; i<cls.size(); i++)
  {
    if(first_indent)
    {
      ret += indented(cls[i]->generate(ind), ind);
    }
    else
    {
      first_indent=true;
      ret += cls[i]->generate(ind);
    }
  }
  return ret;
}

bool add_include(std::string const& file)
{
  std::string truepath=ztd::exec("readlink", "-f", file).first;
  for(auto it: included)
  {
    if(it == truepath)
      return false;
  }
  included.push_back(truepath);
  return true;
}

// BLOCK

std::string block::generate_redirs(int ind)
{
  std::string ret;
  if(redirs != nullptr)
  {
    std::string t = redirs->generate(ind);
    if(t!="")
    {
      if(!opt_minimize) ret += ' ';
      ret += t;
    }
  }
  return ret;
}

std::string if_block::generate(int ind)
{
  std::string ret;

  for(uint32_t i=0; i<blocks.size(); i++ )
  {
    // condition
    if(i==0)
      ret += "if";
    else
      ret += "elif";

    if(blocks[i].first->size()==1)
      ret += ' ' + blocks[i].first->generate(ind+1, false);
    else
      ret += '\n' + blocks[i].first->generate(ind+1);

    // execution
    ret += indented("then\n", ind);
    ret += blocks[i].second->generate(ind+1);
  }

  if(else_lst!=nullptr)
  {
    ret += indented("else\n", ind);
    ret += else_lst->generate(ind+1);
  }

  ret += indented("fi", ind);
  return ret;
}

std::string for_block::generate(int ind)
{
  std::string ret;

  ret += "for "+varname;
  if(iter != nullptr)
    ret += " in " + iter->generate(ind);
  ret += '\n';
  ret += indented("do\n", ind);
  ret += ops->generate(ind+1);
  ret += indented("done", ind);

  return ret;
}

std::string while_block::generate(int ind)
{
  std::string ret;

  ret += "while";
  if(cond->size() == 1)
    ret += " " + cond->generate(ind+1, false);
  else
    ret += '\n' + cond->generate(ind+1);

  ret += indented("do\n", ind);
  ret += ops->generate(ind+1);
  ret += indented("done", ind);

  return ret;
}

std::string subshell::generate(int ind)
{
  std::string ret;
  // open subshell
  ret += '(';
  if(!opt_minimize) ret += '\n';
  // commands
  ret += lst->generate(ind+1);
  if(opt_minimize && ret.size()>1)
    ret.pop_back(); // ) can be right after command
  // close subshell
  ret += indented(")", ind);

  ret += generate_redirs(ind);

  return ret;
}

std::string shmain::generate(int ind)
{
  return this->generate(false, ind);
}
std::string shmain::generate(bool print_shebang, int ind)
{
  std::string ret;
  if(print_shebang && shebang!="")
    ret += shebang + '\n';
  ret += lst->generate(ind);
  if( opt_minimize && ret[ret.size()-1] == '\n')
    ret.pop_back();
  return ret;
}

std::string brace::generate(int ind)
{
  std::string ret;

  ret += "{\n" ;
  ret += lst->generate(ind+1);
  ret += indented("}", ind);

  ret += generate_redirs(ind);

  return ret;
}

std::string function::generate(int ind)
{
  std::string ret;
  // function definition
  ret += name + "()";
  if(!opt_minimize) ret += '\n';
  // commands
  ret += indented("{\n", ind);
  ret += lst->generate(ind+1);
  ret += indented("}", ind);

  ret += generate_redirs(ind);

  return ret;
}

std::string case_block::generate(int ind)
{
  std::string ret;
  ret += "case " + carg->generate(ind) + " in\n";
  ind++;
  for(auto cs: this->cases)
  {
    // case definition : foo)
    ret += indented("", ind);
    // args
    for(auto it: cs.first)
      ret += it->generate(ind) + '|';
    ret.pop_back();
    ret += ')';
    if(!opt_minimize) ret += '\n';
    // commands
    ret += cs.second->generate(ind+1);
    // end of case: ;;
    if(opt_minimize && ret[ret.size()-1] == '\n') // ;; can be right after command
      ret.pop_back();
    ret += indented(";;\n", ind+1);
  }

  // remove ;; from last case
  if(opt_minimize)
  {
    ret.erase(ret.size()-3, 2);
  }

  // close case
  ind--;
  ret += indented("esac", ind);

  ret += generate_redirs(ind);

  return ret;
}

std::string cmd::generate(int ind)
{
  std::string ret;
  // var assigns
  for(auto it: var_assigns)
    ret += it.first + '=' + it.second->generate(ind) + ' ';

  if(args==nullptr || args->size()<=0)
  {
    ret.pop_back();
    return ret;
  }

  // command
  ret += args->generate(ind);
  // delete potential trailing space
  if(ret[ret.size()-1] == ' ')
    ret.pop_back();

  return ret;
}

// SUBARG

std::string subshell_subarg::generate(int ind)
{
  std::string ret;
  ret += '$';
  ret += sbsh->generate(ind);
  return ret;
}

// TEMPLATE

// std::string thing::generate(int ind)
// {
//   std::string ret;
//   return ret;
// }
