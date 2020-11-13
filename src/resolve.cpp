#include "resolve.hpp"

#include <unistd.h>
#include <ztd/shell.hpp>

#include "recursive.hpp"
#include "options.hpp"
#include "util.hpp"
#include "parse.hpp"

std::vector<std::string> included;

// -- CD STUFF --

std::string pwd()
{
  char buf[2048];
  if(getcwd(buf, 2048) == NULL)
  {
    throw std::runtime_error("getcwd failed with errno "+std::to_string(errno));
  }
  return std::string(buf);
}

bool add_include(std::string const& file)
{
  std::string truepath;
  if(file[0] == '/')
    truepath = file;
  else
    truepath=pwd() + '/' + file;
  for(auto it: included)
  {
    if(it == truepath)
      return false;
  }
  included.push_back(truepath);
  return true;
}

// returns path to old dir
std::string _pre_cd(shmain* parent)
{
  if(parent->is_dev_file() || parent->filename == "")
    return "";
  std::string dir=pwd();
  std::string cddir=dirname(parent->filename);
  if(chdir(cddir.c_str()) != 0)
    throw std::runtime_error("Cannot cd to '"+cddir+"'");
  return dir;
}

void _cd(std::string const& dir)
{
  if(dir!="" && chdir(dir.c_str()) != 0)
    throw std::runtime_error("Cannot cd to '"+dir+"'");
}

// -- COMMANDS --

// return <name, contents>[]
std::vector<std::pair<std::string, std::string>> do_include_raw(condlist* cmd, shmain* parent, std::string* ex_dir=nullptr)
{
  std::vector<std::pair<std::string, std::string>> ret;

  ztd::option_set opts = create_include_opts();
  std::vector<std::string> rargs;
  try
  {
    rargs = opts.process(cmd->first_cmd()->args->strargs(1), false, true, false);
  }
  catch(ztd::option_error& e)
  {
    throw std::runtime_error(std::string("%include: ")+e.what());
  }

  std::string dir;
  if(g_cd && !opts['C'])
  {
    dir=_pre_cd(parent);
    if(ex_dir!=nullptr)
      *ex_dir=dir;
  }

  std::string command="for I in ";
  for(auto it: rargs)
    command += it + ' ';
  command += "; do echo $I ; done";
  std::string inc=ztd::sh(command); /* takes 1ms */

  auto v = split(inc, '\n');

  for(auto it: v)
  {
    if(opts['f'] || add_include(it))
    {
      ret.push_back(std::make_pair(it, import_file(it)));
    }
  }

  if(ex_dir==nullptr)
    _cd(dir);

  return ret;
}

std::vector<condlist*> do_include_parse(condlist* cmd, shmain* parent)
{
  std::vector<condlist*> ret;

  std::string dir;
  auto incs=do_include_raw(cmd, parent, &dir);

  for(auto it: incs)
  {
    shmain* sh=parse_text(it.second, it.first);
    resolve(sh);
    // get the cls
    ret.insert(ret.end(), sh->lst->cls.begin(), sh->lst->cls.end());
    // safety and cleanup
    sh->lst->cls.resize(0);
    delete sh;
  }
  // cd back
  _cd(dir);

  return ret;
}

//
std::pair<std::string, std::string> do_resolve_raw(condlist* cmd, shmain* parent, std::string* ex_dir=nullptr)
{
  std::pair<std::string, std::string> ret;

  ztd::option_set opts = create_resolve_opts();
  std::vector<std::string> rargs;
  try
  {
    rargs = opts.process(cmd->first_cmd()->args->strargs(1), false, true, false);
  }
  catch(ztd::option_error& e)
  {
    throw std::runtime_error(std::string("%resolve: ")+e.what());
  }

  std::string dir;
  if(g_cd && !opts['C'])
  {
    dir=_pre_cd(parent);
    if(ex_dir!=nullptr)
      *ex_dir=dir;
  }

  cmd->prune_first_cmd();

  std::string fullcmd=concatargs(rargs);
  std::string othercmd=cmd->generate(0);
  if(othercmd != "")
    fullcmd += '|' + othercmd;

  auto p=ztd::shp(fullcmd);

  if(!opts['f'] && p.second!=0)
  {
    throw std::runtime_error(  strf("command `%s` returned %u", fullcmd.c_str(), p.second) );
  }

  if(ex_dir==nullptr)
    _cd(dir);

  while(p.first[p.first.size()-1] == '\n')
    p.first.pop_back();

  ret = std::make_pair(fullcmd, p.first);
  return ret;
}

// if first is nullptr: is a string
std::vector<condlist*> do_resolve_parse(condlist* cmd, shmain* parent)
{
  std::vector<condlist*> ret;

  std::pair<std::string,std::string> p;
  try
  {
    // get
    std::string dir;
    p=do_resolve_raw(cmd, parent, &dir);
    // do parse
    shmain* sh = parse_text(p.second);
    resolve(sh);
    // get the cls
    ret = sh->lst->cls;
    // safety and cleanup
    sh->lst->cls.resize(0);
    delete sh;
    // cd back
    _cd(dir);
  }
  catch(ztd::format_error& e)
  {
    throw ztd::format_error(e.what(), '`'+p.first+'`', e.data(), e.where());
  }

  return ret;
}

// -- OBJECT CALLS --

std::pair< std::vector<condlist*> , bool > resolve_condlist(condlist* in, shmain* parent)
{
  cmd* tc = in->first_cmd();
  if(tc == nullptr)
    return std::make_pair(std::vector<condlist*>(), false);

  std::string const& strcmd=tc->firstarg_string();

  if(g_include && strcmd == "%include")
    return std::make_pair(do_include_parse(in, parent), true);
  else if(g_resolve && strcmd == "%resolve")
    return std::make_pair(do_resolve_parse(in, parent), true);
  else
    return std::make_pair(std::vector<condlist*>(), false);
}

std::pair< std::vector<arg*> , bool > resolve_arg(arg* in, shmain* parent, bool forcequote=false)
{
  std::vector<arg*> ret;
  arg* ta=nullptr;
  bool has_resolved=false;
  uint32_t j=0;
  for(uint32_t i=0 ; i<in->sa.size() ; i++)
  {
    if(in->sa[i]->type != _obj::subarg_subshell) // skip if not subshell
      continue;

    subshell_subarg* tsh = dynamic_cast<subshell_subarg*>(in->sa[i]);
    if(tsh->sbsh->lst->cls.size() > 1) // skip if more than one cl
      continue;
    condlist* tc = tsh->sbsh->lst->cls[0];
    cmd* c = tc->first_cmd();
    if(c == nullptr) // skip if not cmd
      continue;
    std::string strcmd=c->firstarg_string();
    std::string fulltext;
    if(g_include && strcmd == "%include")
    {
      for(auto it: do_include_raw(tc, parent) )
        fulltext += it.second;
    }
    else if(g_resolve && strcmd == "%resolve")
    {
      fulltext = do_resolve_raw(tc, parent).second;
    }
    else // skip
      continue;

    // start of resolve
    has_resolved = true;

    if(tsh->quoted || forcequote)
    {
      stringReplace(fulltext, "\"", "\\\"");
      stringReplace(fulltext, "!", "\\!");
    }
    if(!tsh->quoted && forcequote)
      fulltext = '"' + fulltext + '"';


    if(tsh->quoted || forcequote)
    {
      // replace with new subarg
      delete in->sa[i];
      in->sa[i] = new string_subarg(fulltext);
    }
    else
    {
      auto strargs=split(fulltext, SEPARATORS);
      if(strargs.size() <= 1)
      {
        std::string val;
        if(strargs.size() == 1)
          val = strargs[0];
        delete in->sa[i];
        in->sa[i] = new string_subarg(val);
      }
      else // pack
      {
        if(ta == nullptr)
          ta = new arg;
        ta->sa.insert(ta->sa.end(), in->sa.begin()+j, in->sa.begin()+i);
        ta->sa.push_back(new string_subarg(strargs[i]));
        j=i+1;
        delete in->sa[i];
        for(uint32_t li=1 ; li<strargs.size() ; li++)
        {
          ret.push_back(ta);
          ta = new arg;
          ta->sa.push_back(new string_subarg(strargs[li]));
        }

      } // end pack

    } // end non quoted

  } // end for
  if(ta != nullptr)
  {
    ta->sa.insert(ta->sa.end(), in->sa.begin()+j, in->sa.end());
    if(ta->sa.size() > 0)
      ret.push_back(ta);
    else
      delete ta;
    in->sa.resize(0);
  }
  return std::make_pair(ret, has_resolved);
}


// -- RECURSIVE CALL --

bool resolve_recurse(_obj* o, shmain* parent)
{
  switch(o->type)
  {
    // in case of applicable object:
    // check every sub-object
    // execute resolve manually
    // instruct parent resolve to not resolve
    case _obj::_list :
    {
      auto t = dynamic_cast<list*>(o);
      for(uint32_t i=0 ; i<t->cls.size() ; i++)
      {
        auto r=resolve_condlist(t->cls[i], parent);
        if(r.second)
        {
          // add new cls after current
          t->cls.insert(t->cls.begin()+i+1, r.first.begin(), r.first.end());
          // delete current
          delete t->cls[i];
          t->cls.erase(t->cls.begin()+i);
          // skip to after inserted cls
          i += r.first.size()-1;
        }
        else
        {
          resolve(t->cls[i], parent);
        }
      }
      return false;
    } break;
    case _obj::_arglist :
    {
      auto t = dynamic_cast<arglist*>(o);
      for(uint32_t i=0 ; i<t->args.size() ; i++)
      {
        auto r=resolve_arg(t->args[i], parent);
        if(r.first.size()>0)
        {
          // add new args
          t->args.insert(t->args.begin()+i+1, r.first.begin(), r.first.end());
          // delete current
          delete t->args[i];
          t->args.erase(t->args.begin()+i);
          i += r.first.size()-1;
        }
        else
        {
          resolve(t->args[i], parent);
        }
      }
      return false;
    } break;
    case _obj::block_cmd :
    {
      auto t = dynamic_cast<cmd*>(o);
      for(auto it: t->var_assigns) // var assigns
      {
        resolve_arg(it.second, parent, true); // force quoted
        resolve(it.second, parent);
      }
      resolve(t->redirs, parent);
      resolve(t->args, parent);
      return false;
    }; break;
    case _obj::block_case :
    {
      auto t = dynamic_cast<case_block*>(o);
      for(auto sc: t->cases)
      {
        resolve_arg(t->carg, parent, true); // force quoted
        resolve(t->carg, parent);

        for(auto it: sc.first)
        {
          resolve_arg(it, parent, true); // force quoted
          resolve(it, parent);
        }
        resolve(sc.second, parent);
      }
    }; break;
    default: break;
  }
  return true;
}

// recursive call of resolve
void resolve(_obj* in, shmain* parent)
{
  recurse(resolve_recurse, in, parent);
}

void resolve(shmain* sh)
{
  recurse(resolve_recurse, sh, sh);
}