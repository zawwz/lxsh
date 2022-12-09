#include "struc.hpp"

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"
#include "parse.hpp"

// global
bool prev_is_heredoc=false;

bool is_sub_special_cmd(std::string in)
{
  return in == "%include_sub" || in == "%resolve_sub";
}

std::string indented(std::string const& in, uint32_t ind)
{
  if(!opt_minify)
    return indent(ind) + in;
  else
    return in;
}

std::string arg_t::generate(int ind)
{
  std::string ret;
  for(auto it: sa)
  {
    ret += it->generate(ind);
  }
  return ret;
}

std::string arglist_t::generate(int ind)
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

std::string pipeline_t::generate(int ind, generate_context* ctx)
{
  std::string ret;

  if(cmds.size()<=0)
    return "";

  if(negated)
    ret += "! ";

  if(bash_time)
    ret += "time ";

  ret += cmds[0]->generate(ind, ctx);
  for(uint32_t i=1 ; i<cmds.size() ; i++)
  {
    ret += opt_minify ? "|" : " | " ;
    ret += cmds[i]->generate(ind, ctx);
  }

  return ret;
}

std::string condlist_t::generate(int ind)
{
  std::string ret;
  if(pls.size() <= 0)
    return "";
  generate_context ctx;
  ret += pls[0]->generate(ind, &ctx);
  for(uint32_t i=0 ; i<pls.size()-1 ; i++)
  {
    if(or_ops[i])
      ret += opt_minify ? "||" : " || ";
    else
      ret += opt_minify ? "&&" : " && ";
    ret += pls[i+1]->generate(ind, &ctx);
  }
  prev_is_heredoc=false;
  if(ret=="")
    return "";
  if(ctx.here_document != nullptr)
  {
    if(parallel)
      ret += '&';
    ret += '\n';
    ret += ctx.here_document->generate(0);
    ret += '\n';
    prev_is_heredoc=true;
  }
  else if(parallel)
  {
    ret += opt_minify ? "&" : " &\n";
  }
  else
    ret += '\n';
  return ret;
}

std::string list_t::generate(int ind, bool first_indent)
{
  std::string ret;
  if(cls.size() <= 0)
    return "";

  std::string next;
  for(uint32_t i=0; i<cls.size(); i++)
  {
    if(first_indent)
    {
      next = indented(cls[i]->generate(ind), ind);
    }
    else
    {
      first_indent=true;
      next = cls[i]->generate(ind);
    }
    if(ret[ret.size()-1] == '&' && next.size()>0 && is_in(next[0], "<>"))
      ret += '\n';
    ret += next;
  }
  return ret;
}

std::string redirect_t::generate(int ind)
{
  std::string ret=op;
  if(target!=nullptr)
  {
    std::string targetret=target->generate(0);
    if(!(opt_minify && !is_in(targetret[0], "<>")))
      ret += ' ';
    ret += targetret;
  }
  return ret;
}

// BLOCK

std::string block_t::generate_redirs(int ind, std::string const& _str, generate_context* ctx=nullptr)
{
  std::string ret=" ";
  bool previous_isnt_num = _str.size()>0 && !is_num(_str[_str.size()-1]);
  for(auto it: redirs)
  {
    if(ctx != nullptr && it->here_document != nullptr)
    {
      if(ctx->here_document != nullptr)
        throw std::runtime_error("Unsupported generation of concurrent here documents");
      ctx->here_document = it->here_document;
    }
    std::string _r = it->generate(0);
    if(opt_minify && _r.size() > 0 && !is_num(_r[0]) && previous_isnt_num)
      ret.pop_back(); // remove one space if possible
    ret += _r + ' ';
    previous_isnt_num = ret.size()>1 && !is_num(ret[ret.size()-2]);
  }
  ret.pop_back(); // remove last space
  return ret;
}

std::string if_t::generate(int ind, generate_context* ctx)
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

  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string for_t::generate(int ind, generate_context* ctx)
{
  std::string ret;

  ret += "for "+var->generate(ind);
  if(in_val) {
    ret += " in";
    if(iter != nullptr)
      ret += " " + iter->generate(ind);
  }
  ret += '\n';
  ret += indented("do\n", ind);
  ret += ops->generate(ind+1);
  ret += indented("done", ind);

  if(opt_minify && ret.size()>1 && !is_alpha(ret[ret.size()-2]))
    ret.pop_back();
  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string while_t::generate(int ind, generate_context* ctx)
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

  if(opt_minify && ret.size()>1 && !is_alpha(ret[ret.size()-2]))
    ret.pop_back();
  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string subshell_t::generate(int ind, generate_context* ctx)
{
  std::string ret;
  // open subshell
  ret += '(';
  if(!opt_minify) ret += '\n';
  // commands
  ret += lst->generate(ind+1);
  if(opt_minify && ret.size()>1)
    ret.pop_back(); // ) can be right after command
  // close subshell
  ret += indented(")", ind);

  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string shmain::generate(int ind, generate_context* ctx)
{
  return this->generate(false, ind);
}
std::string shmain::generate(bool print_shebang, int ind)
{
  std::string ret;
  if(print_shebang && shebang!="")
    ret += shebang + '\n';
  ret += lst->generate(ind);
  if( opt_minify && ret[ret.size()-1] == '\n')
    ret.pop_back();

  return ret;
}

std::string brace_t::generate(int ind, generate_context* ctx)
{
  std::string ret;

  ret += "{\n" ;
  ret += lst->generate(ind+1);
  ret += indented("}", ind);

  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string function_t::generate(int ind, generate_context* ctx)
{
  std::string ret;
  // function definition
  ret += name + "()";
  if(!opt_minify) ret += '\n';
  // commands
  ret += indented("{\n", ind);
  ret += lst->generate(ind+1);
  ret += indented("}", ind);

  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string case_t::generate(int ind, generate_context* ctx)
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
    if(!opt_minify) ret += '\n';
    // commands
    ret += cs.second->generate(ind+1);
    // end of case: ;;
    if(opt_minify && !prev_is_heredoc && ret[ret.size()-1] == '\n') // ;; can be right after command
      ret.pop_back();
    ret += indented(";;", ind+1);
    if(!opt_minify)
      ret+="\n";
  }

  // replace ;; from last case with \n
  if(this->cases.size()>0 && opt_minify)
  {
    ret.pop_back();
    ret.pop_back();
    ret+='\n';
  }

  // close case
  ind--;
  ret += indented("esac", ind);

  ret += generate_redirs(ind, ret, ctx);
  return ret;
}

std::string cmd_t::generate(int ind, generate_context* ctx)
{
  std::string ret;

  bool has_args=false;

  // pre-cmd var assigns
  for(auto it: var_assigns)
  {
    has_args=true;
    if(it.first != nullptr)
      ret += it.first->generate(ind);
    if(it.second != nullptr)
      ret += it.second->generate(ind);
    ret += ' ';
  }

  // is a varassign cmd
  if(is_cmdvar)
  {
    ret += args->generate(ind) + ' ';
    for(auto it: cmd_var_assigns)
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

  // cmd itself
  if(args!=nullptr && args->size()>0)
  {
    has_args=true;
    // command
    ret += args->generate(ind);
    // delete potential trailing space
    if(ret.size()>2 && ret[ret.size()-1] == ' ' && ret[ret.size()-2] != '\\')
      ret.pop_back();
  }
  else // empty command: remove trailing space
  {
    if(ret.size()>0)
      ret.pop_back();
  }

  std::string redirs = generate_redirs(ind, ret, ctx);
  if(!has_args)
    redirs.erase(redirs.begin());
  ret += redirs;
  return ret;
}

// SUBARG

std::string subarg_subshell_t::generate(int ind)
{
  std::string r = sbsh->generate(ind);
  if(backtick) {
    r[0] = '`';
    r[r.size()-1] = '`';
    return r;
  }
  else
    return '$' + r;
}

std::string subarg_procsub_t::generate(int ind)
{
  if(is_output)
    return '>' + sbsh->generate(ind);
  else
    return '<' + sbsh->generate(ind);
}

std::string subarg_arithmetic_t::generate(int ind)
{
  std::string ret;
  ret += "$((";
  if(!opt_minify) ret += ' ';
  ret += arith->generate(ind);
  if(!opt_minify) ret += ' ';
  ret += "))";
  return ret;
}

// ARITHMETIC

std::string arithmetic_operation_t::generate(int ind)
{
  std::string ret;
  if(precedence)
  {
    ret += oper;
    if(!opt_minify) ret += ' ';
    ret += val1->generate(ind);
  }
  else
  {
    ret += val1->generate(ind);
    if(!opt_minify) ret += ' ';
    ret += oper;
    if(!opt_minify) ret += ' ';
    ret += val2->generate(ind);
  }
  return ret;
}

std::string arithmetic_parenthesis_t::generate(int ind)
{
  std::string ret;
  ret += '(';
  if(!opt_minify) ret += ' ';
    ret += val->generate(ind);
  if(!opt_minify) ret += ' ';
    ret += ')';
  return ret;
}

std::string arithmetic_subshell_t::generate(int ind)
{
  return '$' + sbsh->generate(ind);
}

std::string arithmetic_variable_t::generate(int ind)
{
  std::string ret=var->generate(ind);
  if(is_num(ret[0]) || is_in(ret[0], SPECIAL_VARS) || var->is_manip)
    return '$' + ret;
  return ret;
}

std::string variable_t::generate(int ind)
{
  std::string ret;
  if(is_manip)
  {
    ret += '{';
    if(precedence && manip!=nullptr)
      ret += manip->generate(ind);
  }
  ret += varname;
  if(index!=nullptr)
    ret += '[' + index->generate(ind) + ']';
  if(is_manip)
  {
    if(!precedence && manip!=nullptr)
      ret += manip->generate(ind);
    ret += '}';
  }
  return ret;
}


// TEMPLATE

// std::string thing::generate(int ind)
// {
//   std::string ret;
//   return ret;
// }
