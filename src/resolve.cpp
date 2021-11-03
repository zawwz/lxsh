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
std::string _pre_cd(std::string const& filename)
{
  if(filename == "" || is_dev_file(filename))
    return "";
  std::string dir=pwd();
  std::string cddir=dirname(filename);
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
std::vector<std::pair<std::string, std::string>> do_include_raw(condlist_t* cmd, parse_context ctx, std::string* ex_dir)
{
  std::vector<std::pair<std::string, std::string>> ret;

  ztd::option_set opts = create_include_opts();
  std::vector<std::string> rargs;
  try
  {
    rargs = opts.process(cmd->first_cmd()->args->strargs(1), {.stop_on_argument=true});
  }
  catch(ztd::option_error& e)
  {
    throw std::runtime_error(std::string("%include: ")+e.what());
  }

  std::string dir;
  if(g_cd && !opts['C'])
  {
    dir=_pre_cd(ctx.filename);
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

//
std::pair<std::string, std::string> do_resolve_raw(condlist_t* cmd, parse_context ctx, std::string* ex_dir)
{
  std::pair<std::string, std::string> ret;

  ztd::option_set opts = create_resolve_opts();
  std::vector<std::string> rargs;
  try
  {
    rargs = opts.process(cmd->first_cmd()->args->strargs(1), {.stop_on_argument=true} );
  }
  catch(ztd::option_error& e)
  {
    throw std::runtime_error(std::string("%resolve: ")+e.what());
  }

  std::string dir;
  if(g_cd && !opts['C'])
  {
    dir=_pre_cd(ctx.filename);
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

std::vector<condlist_t*> do_include_parse(condlist_t* cmd, parse_context ctx)
{
  std::vector<condlist_t*> ret;

  std::string dir;
  auto incs=do_include_raw(cmd, ctx, &dir);

  std::vector<shmain*> shs;
  shs.resize(incs.size());

  for(uint32_t i=0; i<incs.size(); i++)
  {
    parse_context newctx = make_context(ctx, incs[i].second, incs[i].first);
    auto pp = parse_text(newctx);
    shmain* sh = pp.first;
    resolve(sh, pp.second);
    shs[i] = sh;
  }
  for(auto sh: shs)
  {
    // get the cls
    ret.insert(ret.end(), sh->lst->cls.begin(), sh->lst->cls.end());
    // safety and cleanup
    sh->lst->cls.resize(0);
    delete sh;
  }
  shs.resize(0);
  // cd back
  _cd(dir);

  return ret;
}

// if first is nullptr: is a string
std::vector<condlist_t*> do_resolve_parse(condlist_t* cmd, parse_context ctx)
{
  std::vector<condlist_t*> ret;

  std::pair<std::string,std::string> p;
  try
  {
    // get
    std::string dir;
    p=do_resolve_raw(cmd, ctx, &dir);

    // do parse
    parse_context newctx = make_context(ctx, p.second, '`'+p.first+'`');
    auto pp = parse_text(newctx);
    shmain* sh = pp.first;
    resolve(sh, pp.second);
    // get the cls
    ret = sh->lst->cls;
    // safety and cleanup
    sh->lst->cls.resize(0);
    delete sh;
    // cd back
    _cd(dir);
  }
  catch(format_error& e)
  {
    throw format_error(e.what(), '`'+p.first+'`', e.data(), e.where());
  }

  return ret;
}

// -- OBJECT CALLS --

std::pair< std::vector<condlist_t*> , bool > resolve_condlist(condlist_t* in, parse_context ctx)
{
  cmd_t* tc = in->first_cmd();
  if(tc == nullptr)
    return std::make_pair(std::vector<condlist_t*>(), false);

  std::string const& strcmd=tc->arg_string(0);

  if(g_include && strcmd == "%include")
    return std::make_pair(do_include_parse(in, ctx), true);
  else if(g_resolve && strcmd == "%resolve")
    return std::make_pair(do_resolve_parse(in, ctx), true);
  else
    return std::make_pair(std::vector<condlist_t*>(), false);
}

std::pair< std::vector<arg_t*> , bool > resolve_arg(arg_t* in, parse_context ctx, bool forcequote=false)
{
  std::vector<arg_t*> ret;
  if(in == nullptr)
  {
    return std::make_pair(ret, false);
  }
  arg_t* ta=nullptr;
  bool has_resolved=false;
  uint32_t j=0;
  for(uint32_t i=0 ; i<in->size() ; i++)
  {
    if(in->sa[i]->type != _obj::subarg_subshell) // skip if not subshell
      continue;

    subarg_subshell_t* tsh = dynamic_cast<subarg_subshell_t*>(in->sa[i]);
    if(tsh->sbsh->lst->cls.size() != 1) // skip if not one cl
      continue;
    condlist_t* tc = tsh->sbsh->lst->cls[0];
    cmd_t* c = tc->first_cmd();
    if(c == nullptr) // skip if not cmd
      continue;
    std::string strcmd=c->arg_string(0);
    std::string fulltext;
    if(g_include && strcmd == "%include")
    {
      for(auto it: do_include_raw(tc, ctx) )
        fulltext += it.second;
    }
    else if(g_resolve && strcmd == "%resolve")
    {
      fulltext = do_resolve_raw(tc, ctx).second;
    }
    else // skip
      continue;

    // start of resolve
    has_resolved = true;

    if(tsh->quoted || forcequote)
    {
      fulltext = stringReplace(fulltext, "\\\"", "\\\\\"");
      fulltext = escape_chars(fulltext, "\"`$");
      fulltext = stringReplace(fulltext, "!", "\"\\!\"");
    }
    else
    {
      fulltext = escape_chars(fulltext, "\\\"!$`#&|()';<>");
    }

    if(!tsh->quoted && forcequote)
     fulltext = '"' + fulltext + '"';


    if(tsh->quoted || forcequote)
    {
      // replace with new subarg
      delete in->sa[i];
      in->sa[i] = new subarg_string_t(fulltext);
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
        in->sa[i] = new subarg_string_t(val);
      }
      else // pack
      {
        if(ta == nullptr)
          ta = new arg_t;
        ta->sa.insert(ta->sa.end(), in->sa.begin()+j, in->sa.begin()+i);
        ta->add(new subarg_string_t(strargs[i]));
        j=i+1;
        delete in->sa[i];
        for(uint32_t li=1 ; li<strargs.size() ; li++)
        {
          ret.push_back(ta);
          ta = new arg_t;
          ta->add(new subarg_string_t(strargs[li]));
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

void resolve(_obj* in, parse_context* ctx);
// -- RECURSIVE CALL --

bool r_resolve(_obj* o, parse_context* ct)
{
  switch(o->type)
  {
    // in case of applicable object:
    // check every sub-object
    // execute resolve manually
    // instruct parent resolve to not resolve
    case _obj::list :
    {
      auto t = dynamic_cast<list_t*>(o);
      for(uint32_t i=0 ; i<t->cls.size() ; i++)
      {
        auto r=resolve_condlist(t->cls[i], *ct);
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
          resolve(t->cls[i], ct);
        }
      }
      return false;
    } break;
    case _obj::arglist :
    {
      auto t = dynamic_cast<arglist_t*>(o);
      for(uint32_t i=0 ; i<t->size() ; i++)
      {
        auto r=resolve_arg(t->args[i], *ct);
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
          resolve(t->args[i], ct);
        }
      }
      return false;
    } break;
    case _obj::block_cmd :
    {
      auto t = dynamic_cast<cmd_t*>(o);
      for(auto it: t->var_assigns) // var assigns
      {
        resolve_arg(it.second, *ct, true); // force quoted
        resolve(it.second, ct);
      }
      for(auto it: t->cmd_var_assigns) // var assigns
      {
        resolve_arg(it.second, *ct, true); // force quoted
        resolve(it.second, ct);
      }
      for(auto it: t->redirs)
        resolve(it, ct);
      resolve(t->args, ct);
      return false;
    }; break;
    case _obj::block_case :
    {
      auto t = dynamic_cast<case_t*>(o);
      for(auto sc: t->cases)
      {
        resolve_arg(t->carg, *ct, true); // force quoted
        resolve(t->carg, ct);

        for(auto it: sc.first)
        {
          resolve_arg(it, *ct, true); // force quoted
          resolve(it, ct);
        }
        resolve(sc.second, ct);
      }
    }; break;
    default: break;
  }
  return true;
}

// recursive call of resolve
void resolve(_obj* in, parse_context* ctx)
{
  recurse(r_resolve, in, ctx);
}

// recursive call of resolve
void resolve(_obj* in, parse_context ctx)
{
  recurse(r_resolve, in, &ctx);
}
