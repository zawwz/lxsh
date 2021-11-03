#include "exec.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#include "g_shellcode.h"

#include "util.hpp"
#include "parse.hpp"
#include "debashify.hpp"
#include "resolve.hpp"
#include "recursive.hpp"
#include "shellcode.hpp"

#define PIPE_READ  0
#define PIPE_WRITE 1

std::vector<condlist_t*> do_include_exec(condlist_t* cmd, parse_context ctx, FILE* fd)
{
  std::vector<condlist_t*> ret;

  std::string dir;
  auto incs=do_include_raw(cmd, ctx, &dir);

  for(auto it: incs)
  {
    parse_exec(fd, make_context(ctx, it.second, it.first));
  }
  // cd back
  _cd(dir);

  return ret;
}

// if first is nullptr: is a string
std::vector<condlist_t*> do_resolve_exec(condlist_t* cmd, parse_context ctx, FILE* fd)
{
  std::vector<condlist_t*> ret;

  std::pair<std::string,std::string> p;
  try
  {
    // get
    std::string dir;
    p=do_resolve_raw(cmd, ctx, &dir);
    // do parse
    parse_exec(fd, make_context(ctx, p.second, p.first));
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

bool resolve_condlist_exec(condlist_t* in, parse_context ctx, FILE* fd)
{
  cmd_t* tc = in->first_cmd();
  if(tc == nullptr)
    return false;

  std::string const& strcmd=tc->arg_string(0);

  if(g_include && strcmd == "%include")
  {
    do_include_exec(in, ctx, fd);
    return true;
  }
  else if(g_resolve && strcmd == "%resolve")
  {
    do_resolve_exec(in, ctx, fd);
    return true;
  }
  return false;
}


bool resolve_exec(condlist_t* in, parse_context ctx, FILE* fd)
{
  if(!resolve_condlist_exec(in, ctx, fd))
  {
    resolve(in, ctx);
    return false;
  }
  return true;
}

char byte_to_char(uint8_t in)
{
  uint8_t t = in&0b00111111; // equiv to %64
  if(t < 26)
    return t+'a';
  if(t < 52)
    return (t-26)+'A';
  if(t < 62)
    return (t-52)+'0';
  if(t == 62)
    return '-';
  return '_';
}

std::string gettmpdir()
{
  std::string tmpdir;
  char* tbuf = getenv("TMPDIR");
  if(tbuf != NULL)
    tmpdir = tbuf;
  if(tmpdir == "")
    tmpdir = "/tmp";
  return tmpdir;
}

// random string of size 20
std::string random_string()
{
  // get system random seed
  FILE* f = fopen("/dev/urandom", "r");
  if(!f)
    throw std::runtime_error("Cannot open stream to /dev/urandom");
  uint8_t buffer[20];
  size_t r = fread(buffer, 20, 1, f);
  fclose(f);
  if(r<=0)
    throw std::runtime_error("Cannot read from /dev/urandom");

  std::string ret;
  for(uint8_t i=0; i<20; i++)
    ret += byte_to_char(buffer[i]);

  return ret;
}

void parse_exec(FILE* fd, parse_context ctx)
{
  ctx.i=skip_unread(ctx);

  debashify_params debash_params;
  list_t* t_lst=new list_t;
  if(t_lst == nullptr)
    throw std::runtime_error("Alloc error");
  while(ctx.i<ctx.size)
  {
    auto pp=parse_condlist(ctx);
    ctx=pp.second;
    if(ctx.has_errored)
    {
      parse_list_until(ctx);
      throw std::runtime_error("Aborting due to previous errors");
    }
    t_lst->add(pp.first);
    if(g_resolve || g_include)
    {
      if(resolve_exec(t_lst->cls[0], ctx, fd))
      {
        t_lst->clear();
        continue;
      }
    }
    if(options["debashify"])
      debashify(t_lst, &debash_params);


    std::string gen=t_lst->generate(0);
    t_lst->clear();

    fprintf(fd, "%s", gen.c_str());

    if(ctx.i < ctx.size)
    {
      if(ctx[ctx.i] == '#')
        ; // skip here
      else if(is_in(ctx[ctx.i], COMMAND_SEPARATOR))
        ctx.i++; // skip on next char
      else if(is_in(ctx[ctx.i], CONTROL_END))
      {
        format_error(strf("Unexpected token: '%c'", ctx[ctx.i]), ctx);
        return;
      }

      ctx.i = skip_unread(ctx);
    }
  }
  delete t_lst;
}

pid_t forkexec(const char* bin, char *const args[])
{
  pid_t child_pid;
  if((child_pid = vfork()) == -1)
  {
    throw std::runtime_error("fork() failed");
  }
  if (child_pid == 0) // child process
  {
    execv(bin, args);
    throw std::runtime_error("execv() failed");
  }
  else // main process
  {
    return child_pid;
  }
}

int wait_pid(pid_t pid)
{
  int stat;
  while (waitpid(pid, &stat, 0) == -1)
  {
    if (errno != EINTR)
    {
      stat = -1;
      break;
    }
  }

  return WEXITSTATUS(stat);
}

int exec_process(std::string const& runtime, std::vector<std::string> const& args, parse_context ctx)
{
  std::vector<std::string> strargs = split(runtime, " \t");
  std::vector<char*> runargs;

  std::string fifopath=gettmpdir();
  fifopath+="/lxshfiforun_";
  fifopath+=random_string();

  if(mkfifo(fifopath.c_str(), 0700)<0)
    throw std::runtime_error("Cannot create fifo "+fifopath);

  for(uint32_t i=0; i<strargs.size(); i++)
    runargs.push_back((char*) strargs[i].c_str());
  runargs.push_back((char*) fifopath.c_str());
  for(uint32_t i=0; i<args.size(); i++)
    runargs.push_back((char*) args[i].c_str());
  runargs.push_back(NULL);

  pid_t pid=0;
  FILE* ffd=0;
  try
  {
    pid = forkexec(runargs[0], runargs.data());
    ffd = fopen(fifopath.c_str(), "w");
    if(options["debashify"])
    {
      for(auto it: lxsh_array_fcts)
        fprintf(ffd, "%s\n", it.second.code);
    }
    for(auto it: lxsh_extend_fcts)
      fprintf(ffd, "%s\n", it.second.code);
    parse_exec(ffd, ctx);
  }
  catch(std::runtime_error& e)
  {
    fclose(ffd);
    unlink(fifopath.c_str());
    if(pid != 0)
      kill(pid, SIGINT);
    throw e;
  }

  fclose(ffd);
  unlink(fifopath.c_str());

  return wait_pid(pid);
}
