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
  if(!opt_minimize) ret += INDENT;
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
  auto opts=create_resolve_opts();
  auto rargs = opts.process(args, false, true, false);

  std::string cmd=concatargs(rargs);
  auto p=ztd::shp("exec "+cmd);

  if(!opts['f'] && p.second!=0)
  {
    throw std::runtime_error(  strf("command `%s` returned %u", cmd.c_str(), p.second) );
  }
  while(p.first[p.first.size()-1] == '\n')
    p.first.pop_back();

  if(opts['p'])
  {
    block bl = parse(p.first);
    std::string ret = bl.generate(ind, false);
    std::string tmpind=INDENT;
    ret = ret.substr(tmpind.size());
    ret.pop_back(); // remove \n
    return ret;
  }
  else
  {
    return p.first;
  }
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
      if(opts['r'])
        ret += file;
      else
      {
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
    if(!opt_minimize) ret += INDENT;
    ret += cs.first.generate(ind) + ')';
    if(!opt_minimize) ret += '\n';
    for(auto it: cs.second)
      ret += it.generate(ind+1);
    if(opt_minimize)
    {
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
      ret += shebang + "()";
      if(!opt_minimize) ret += '\n' + INDENT;
        ret += "{\n";
      for(auto it: cls)
        ret += it.generate(ind+1);
      if(!opt_minimize)
        ret += INDENT;
      ret += '}';
    }
    else if(type==subshell)
    {
      ret += '(';
      if(!opt_minimize) ret += '\n';
      for(auto it: cls)
        ret += it.generate(ind+1);
      if(opt_minimize)
        ret.pop_back();
      else
        ret += INDENT;
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
