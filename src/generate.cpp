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

std::string arg::generate(int ind)
{
  std::string ret;
  for(auto it: sa)
  {
    ret += it.generate(ind);
  }
  return ret;
}

std::string arglist::generate(int ind)
{
  std::string ret;

  for(auto it: args)
  {
    ret += it.generate(ind);
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

  ret += cmds[0].generate(ind);
  for(uint32_t i=1 ; i<cmds.size() ; i++)
  {
    ret += opt_minimize ? "|" : " | " ;
    ret += cmds[i].generate(ind);
  }

  return ret;
}

std::string condlist::generate(int ind)
{
  std::string ret;
  if(pls.size() <= 0)
    return "";
  if(!opt_minimize)
    ret += INDENT;
  ret += pls[0].generate(ind);
  for(uint32_t i=0 ; i<pls.size()-1 ; i++)
  {
    if(or_ops[i])
      ret += opt_minimize ? "||" : " || ";
    else
      ret += opt_minimize ? "&&" : " && ";
    ret += pls[i+1].generate(ind);
  }
  if(ret=="")
    return "";
  if(parallel)
  {
    ret += opt_minimize ? "&" : "&\n";
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
    block bl;
    try
    {
      bl = parse(p.first);
    }
    catch(ztd::format_error& e)
    {
      throw ztd::format_error(e.what(), "command `"+cmd+'`', e.data(), e.where());
    }
    ret = bl.generate(ind, false);
    std::string tmpind=INDENT;
    ret = ret.substr(tmpind.size());
    ret.pop_back(); // remove \n
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
  block bl;
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
        file = bl.generate(ind, false);
        if(indent_remove)
        {
          indent_remove=false;
          std::string tmpind=INDENT;
          file = file.substr(tmpind.size());
        }
        ret += file;
      }
    }
  }
  if(!opts['C'] && !piped)
    if(chdir(dir.c_str()) != 0)
      throw std::runtime_error("Cannot cd to '"+dir+"'");
  g_origin=curfile;

  if(!opts['r'])
    ret.pop_back();

  return ret;
}

std::string block::generate_cmd(int ind)
{
  std::string ret;
  if(args.size()<=0)
    return "";
  std::string cmd=args[0].raw;
  if(cmd == "%include" || cmd == "%include_s")
  {
    ret += generate_include(args.strargs(1), ind);
  }
  else if(cmd == "%resolve" || cmd == "%resolve_s")
  {
    ret += generate_resolve(args.strargs(1), ind);
  }
  else
    ret = args.generate(ind);
  return ret;
}

std::string block::generate_case(int ind)
{
  std::string ret;
  ret += "case " + carg.generate(ind) + " in\n";
  ind++;
  for(auto cs: this->cases)
  {
    // case definition : foo)
    if(!opt_minimize) ret += INDENT;
    for(auto it: cs.first)
      ret += it.generate(ind) + '|';
    ret.pop_back();
    ret += ')';
    if(!opt_minimize) ret += '\n';
    // commands
    for(auto it: cs.second)
      ret += it.generate(ind+1);
    // end of case: ;;
    if(opt_minimize)
    {
      // ;; can be right after command
      if(ret[ret.size()-1] == '\n')
        ret.pop_back();
    }
    else
    {
      ind++;
      ret += INDENT;
      ind--;
    }
    ret += ";;\n";
  }
  // close case
  ind--;
  if(!opt_minimize) ret += INDENT;
  ret += "esac";
  return ret;
}

std::string block::generate(int ind, bool print_shebang)
{
  std::string ret;

  if(type==cmd)
  {
    ret += generate_cmd(ind);
  }
  else
  {
    if(type==function)
    {
      // function definition
      ret += shebang + "()";
      if(!opt_minimize) ret += '\n' + INDENT;
      ret += "{\n";
      // commands
      for(auto it: cls)
        ret += it.generate(ind+1);
      if(!opt_minimize) ret += INDENT;
      // end function
      ret += '}';
    }
    else if(type==subshell)
    {
      // open subshell
      ret += '(';
      if(!opt_minimize) ret += '\n';
      // commands
      for(auto it: cls)
        ret += it.generate(ind+1);
      if(opt_minimize && ret.size()>1)
        ret.pop_back(); // ) can be right after command
      else
        ret += INDENT;
      // close subshell
      ret += ')';
    }
    else if(type==brace)
    {
      ret += "{\n" ;
      for(auto it: cls)
        ret += it.generate(ind+1);
      if(!opt_minimize)
        ret += INDENT;
      ret += '}';
    }
    else if(type==main)
    {
      if(print_shebang && shebang!="")
        ret += shebang + '\n';
      for(auto it: cls)
        ret += it.generate(ind);
    }
    else if(type==case_block)
    {
      ret += generate_case(ind);
    }

    std::string t = generate_cmd(ind); // leftover redirections
    if(t!="")
    {
      if(!opt_minimize) ret += ' ';
      ret += t;
    }

  }

  return ret;
}

std::string subarg::generate(int ind)
{
  std::string ret;
  if(type == subarg::string)
  {
    ret += val;
  }
  else if(type == subarg::arithmetic)
  {
    ret += "$(("+val+"))";
  }
  else if(type == subarg::subshell)
  {
    // includes and resolves inside command substitutions
    // resolve here and not inside subshell
    block* cmd = sbsh.single_cmd();
    if( cmd != nullptr && (cmd->args[0].raw == "%include" || cmd->args[0].raw == "%resolve") )
    {
      ret += cmd->generate(ind);
    }
    // regular substitution
    else
    {
      ret += '$';
      ret += sbsh.generate(ind);
    }
  }
  return ret;
}
