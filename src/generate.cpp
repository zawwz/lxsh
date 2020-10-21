#include "struc.hpp"

#include <ztd/shell.hpp>

#include "util.hpp"
#include "options.hpp"
#include "parse.hpp"

#include <unistd.h>

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

std::string condlist::generate(int ind, bool pre_indent)
{
  std::string ret;
  if(pls.size() <= 0)
    return "";
  if(pre_indent)
    ret += indented("", ind);
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

std::string concatargs(std::vector<std::string> args)
{
  std::string ret;
  for(auto it: args)
    ret += it + ' ';
  ret.pop_back();
  return ret;
}

std::string generate_resolve(std::vector<std::string> args, int ind)
{
  std::string ret;

  auto opts=create_resolve_opts();
  auto rargs = opts.process(args, false, true, false);

  std::string cmd=concatargs(rargs);
  std::string dir;

  if(!opts['C'] && !piped)
  {
    dir=pwd();
    std::string cddir=ztd::exec("dirname", g_origin).first;
    cddir.pop_back();
    if(chdir(cddir.c_str()) != 0)
      throw std::runtime_error("Cannot cd to '"+cddir+"'");
  }

  // exec call
  auto p=ztd::shp("exec "+cmd);

  if(!opts['f'] && p.second!=0)
  {
    throw std::runtime_error(  strf("command `%s` returned %u", cmd.c_str(), p.second) );
  }
  while(p.first[p.first.size()-1] == '\n')
    p.first.pop_back();

  if(opts['p'])
  {
    shmain* sh;
    try
    {
      sh = parse(p.first);
    }
    catch(ztd::format_error& e)
    {
      throw ztd::format_error(e.what(), "command `"+cmd+'`', e.data(), e.where());
    }
    ret = sh->generate(false, ind);
    delete sh;
    ret = ret.substr(indent(ind).size());
    if(ret[ret.size()-1] != '\n')
      ret += '\n';
  }
  else
  {
    ret = p.first;
  }

  if(!opts['C'] && !piped)
    if(chdir(dir.c_str()) != 0)
      throw std::runtime_error("Cannot cd to '"+dir+"'");

  return ret;
}

std::string generate_include(std::vector<std::string> args, int ind)
{
  std::string ret;

  auto opts=create_include_opts();
  auto rargs = opts.process(args, false, true, false);

  std::string quote;
  if(opts['s'])
    quote = '\'';
  else if(opts['d'])
    quote = '"';

  std::string curfile=g_origin;
  std::string dir;

  if(!opts['C'] && !piped)
  {
    dir=pwd();
    std::string cddir=ztd::exec("dirname", curfile).first;
    cddir.pop_back();
    if(chdir(cddir.c_str()) != 0)
      throw std::runtime_error("Cannot cd to '"+cddir+"'");
  }

  // do shell resolution
  std::string command="for I in ";
  for(auto it: rargs)
    command += it + ' ';
  command += "; do echo $I ; done";
  std::string inc=ztd::sh(command);

  auto v = split(inc, '\n');

  std::string file;
  shmain* bl=nullptr;
  bool indent_remove=true;

  for(auto it : v)
  {
    if( opts['f'] ||      // force include
        add_include(it) ) // not already included
    {
      file=import_file(it);
      if(opts['d'])
        file = stringReplace(file, "\"", "\\\"");
      if(opts['s'])
        file = stringReplace(file, "'", "'\\''");
      if(opts['r'])
        ret += file;
      else
      {
        g_origin=it;
        try
        {
          bl = parse(quote + file + quote);
        }
        catch(ztd::format_error& e)
        {
          throw ztd::format_error(e.what(), it, e.data(), e.where());
        }
        file = bl->generate(false, ind);
        if(file[file.size()-1] != '\n')
          file += '\n';
        delete bl;
        if(indent_remove)
        {
          indent_remove=false;
          file = file.substr(indent(ind).size());
        }
        ret += file;
      }
    }
  }
  if(!opts['C'] && !piped)
    if(chdir(dir.c_str()) != 0)
      throw std::runtime_error("Cannot cd to '"+dir+"'");
  g_origin=curfile;

  return ret;
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
      ret += "if ";
    else
      ret += "elif ";
    // first cmd: on same line with no indent
    ret += blocks[i].first[0]->generate(ind+1, false);
    // other cmds: on new lines
    for(uint32_t j=1; j<blocks[i].first.size(); j++ )
      ret += blocks[i].first[j]->generate(ind+1);

    // execution
    ret += indented("then\n", ind);
    for(auto it: blocks[i].second)
      ret += it->generate(ind+1);
  }

  if(else_cls.size()>0)
  {
    ret += indented("else\n", ind);
    for(auto it: else_cls)
      ret += it->generate(ind+1);
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
  for(auto it: ops)
    ret += it->generate(ind+1);
  ret += indented("done", ind);

  return ret;
}

std::string while_block::generate(int ind)
{
  std::string ret;

  ret += "while";
  if(cond.size() == 1)
  {
    ret += " " + cond[0]->generate(ind+1, false);
  }
  else
  {
    ret += '\n';
    for(auto it: cond)
      ret += it->generate(ind+1);
  }
  ret += indented("do\n", ind);
  for(auto it: ops)
    ret += it->generate(ind+1);
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
  for(auto it: cls)
    ret += it->generate(ind+1);
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
  for(auto it: cls)
    ret += it->generate(ind);
  if( opt_minimize && ret[ret.size()-1] == '\n')
    ret.pop_back();
  return ret;
}

std::string brace::generate(int ind)
{
  std::string ret;
  ret += "{\n" ;
  for(auto it: cls)
    ret += it->generate(ind+1);

  ret += indented("}", ind);

  ret += generate_redirs(ind);

  return ret;
}

std::string function::generate(int ind)
{
  std::string ret;
  // function definition
  ret += name + "()";
  if(!opt_minimize) ret += '\n' + indent(ind);
  ret += "{\n";
  // commands
  for(auto it: cls)
    ret += it->generate(ind+1);
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
    for(auto it: cs.second)
      ret += it->generate(ind+1);
    // end of case: ;;
    if(opt_minimize && ret[ret.size()-1] == '\n') // ;; can be right after command
    {
      ret.pop_back();
    }
    ret += indented(";;\n", ind+1);
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
  if(args==nullptr || args->size()<=0)
    return "";
  std::string cmdname=(*args)[0]->raw;
  if(cmdname == "%include" || cmdname == "%include_s")
  {
    ret += generate_include(args->strargs(1), ind);
  }
  else if(cmdname == "%resolve" || cmdname == "%resolve_s")
  {
    ret += generate_resolve(args->strargs(1), ind);
  }
  else
    ret = args->generate(ind);
  return ret;
}

// TEMPLATE

// std::string thing::generate(int ind)
// {
//   std::string ret;
//   return ret;
// }

// SUBARG

std::string subarg::generate(int ind)
{
  switch(type)
  {
    case subarg::s_string:
      return dynamic_cast<subarg_string*>(this)->generate(ind);
    case subarg::s_arithmetic:
      return dynamic_cast<subarg_arithmetic*>(this)->generate(ind);
    case subarg::s_subshell:
      return dynamic_cast<subarg_subshell*>(this)->generate(ind);
  }
  // doesn't happen, just to get rid of warning
  return "";
}

std::string subarg_subshell::generate(int ind)
{
  std::string ret;
  // includes and resolves inside command substitutions
  // resolve here and not inside subshell
  cmd* cmd = sbsh->single_cmd();
  if( cmd != nullptr && (cmd->firstarg_raw() == "%include" || cmd->firstarg_raw() == "%resolve") )
  {
    ret += cmd->generate(ind);
  }
  // regular substitution
  else
  {
    ret += '$';
    ret += sbsh->generate(ind);
  }
  return ret;
}
