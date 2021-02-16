#include "struc.hpp"

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"
#include "parse.hpp"

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

std::string redirect::generate(int ind)
{
  std::string ret=op;
  if(target!=nullptr)
  {
    if(!opt_minimize)
      ret += ' ';
    ret += target->generate(0);
  }
  return ret;
}

// BLOCK

std::string block::generate_redirs(int ind, std::string const& _str)
{
  std::string ret=" ";
  bool previous_isnt_num = _str.size()>0 && !is_num(_str[_str.size()-1]);
  for(auto it: redirs)
  {
    std::string _r = it->generate(0);
    if(opt_minimize && _r.size() > 0 && !is_num(_r[0]) && previous_isnt_num)
      ret.pop_back(); // remove one space if possible
    ret += _r + ' ';
    previous_isnt_num = ret.size()>1 && !is_num(ret[ret.size()-2]);
  }
  ret.pop_back(); // remove last space
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
      ret += indented("elif", ind);

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

  ret += generate_redirs(ind, ret);
  return ret;
}

std::string for_block::generate(int ind)
{
  std::string ret;

  ret += "for "+var->generate(ind);
  if(iter != nullptr)
    ret += " in " + iter->generate(ind);
  ret += '\n';
  ret += indented("do\n", ind);
  ret += ops->generate(ind+1);
  ret += indented("done", ind);

  if(opt_minimize && ret.size()>1 && !is_alpha(ret[ret.size()-2]))
    ret.pop_back();
  ret += generate_redirs(ind, ret);
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

  if(opt_minimize && ret.size()>1 && !is_alpha(ret[ret.size()-2]))
    ret.pop_back();
  ret += generate_redirs(ind, ret);
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

  ret += generate_redirs(ind, ret);
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

  ret += generate_redirs(ind, ret);
  return ret;
}

std::string brace::generate(int ind)
{
  std::string ret;

  ret += "{\n" ;
  ret += lst->generate(ind+1);
  ret += indented("}", ind);

  ret += generate_redirs(ind, ret);
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

  ret += generate_redirs(ind, ret);
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
  if(this->cases.size()>0 && opt_minimize)
  {
    ret.erase(ret.size()-3, 2);
  }

  // close case
  ind--;
  ret += indented("esac", ind);

  ret += generate_redirs(ind, ret);
  return ret;
}

std::string cmd::generate(int ind)
{
  std::string ret;
  // var assigns
  if(is_cmdvar)
  {
    ret += args->generate(ind) + ' ';
    for(auto it: var_assigns)
    {
      if(it.first != nullptr)
        ret += it.first->generate(ind);
      if(it.second != nullptr)
        ret += it.second->generate(ind);
      ret += ' ';
    }
    ret.pop_back();
    return ret;
  }

  for(auto it: var_assigns)
  {
    if(it.first != nullptr)
      ret += it.first->generate(ind);
    if(it.second != nullptr)
      ret += it.second->generate(ind);
    ret += ' ';
  }

  if(args!=nullptr && args->size()>0)
  {
    // command
    ret += args->generate(ind);
    // delete potential trailing space
    if(ret[ret.size()-1] == ' ')
      ret.pop_back();
  }
  else // empty command: remove trailing space
  {
    if(ret.size()>0)
      ret.pop_back();
  }

  ret += generate_redirs(ind, ret);
  return ret;
}

// SUBARG

std::string subshell_subarg::generate(int ind)
{
  return '$' + sbsh->generate(ind);
}

std::string manipulation_subarg::generate(int ind)
{
  if(size)
    return "${#" + var->generate(ind) + "}";
  else
    return "${" + var->generate(ind) + manip->generate(ind) + "}";
}

std::string procsub_subarg::generate(int ind)
{
  if(is_output)
    return '>' + sbsh->generate(ind);
  else
    return '<' + sbsh->generate(ind);
}

std::string arithmetic_subarg::generate(int ind)
{
  std::string ret;
  ret += "$((";
  if(!opt_minimize) ret += ' ';
  ret += arith->generate(ind);
  if(!opt_minimize) ret += ' ';
  ret += "))";
  return ret;
}

// ARITHMETIC

std::string operation_arithmetic::generate(int ind)
{
  std::string ret;
  if(precedence)
  {
    ret += oper;
    if(!opt_minimize) ret += ' ';
    ret += val1->generate(ind);
  }
  else
  {
    ret += val1->generate(ind);
    if(!opt_minimize) ret += ' ';
    ret += oper;
    if(!opt_minimize) ret += ' ';
    ret += val2->generate(ind);
  }
  return ret;
}

std::string parenthesis_arithmetic::generate(int ind)
{
  std::string ret;
  ret += '(';
  if(!opt_minimize) ret += ' ';
    ret += val->generate(ind);
  if(!opt_minimize) ret += ' ';
    ret += ')';
  return ret;
}

std::string subshell_arithmetic::generate(int ind)
{
  return '$' + sbsh->generate(ind);
}

std::string variable_arithmetic::generate(int ind)
{
  std::string ret=var->generate(ind);
  if(is_num(ret[0]) || is_in(ret[0], SPECIAL_VARS))
    return '$' + ret;
  return ret;
}

std::string variable::generate(int ind)
{
  if(index!=nullptr)
  {
    return varname + '[' + index->generate(ind) + ']';
  }
  else
    return varname;
}


// TEMPLATE

// std::string thing::generate(int ind)
// {
//   std::string ret;
//   return ret;
// }
