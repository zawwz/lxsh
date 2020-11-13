#include <iostream>

#include <string.h>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include <unistd.h>

#include "util.hpp"
#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"
#include "recursive.hpp"
#include "minimize.hpp"
#include "resolve.hpp"

void oneshot_opt_process(const char* arg0)
{
  if(options['h'])
  {
    print_help(arg0);
    exit(1);
  }
  if(options["help-commands"])
  {
    print_include_help();
    printf("\n\n");
    print_resolve_help();
    exit(1);
  }
}

int main(int argc, char* argv[])
{
  std::vector<std::string> args;

  int ret=0;

  try
  {
    args=options.process(argc, argv, false, true);
  }
  catch(std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  // resolve input
  std::string file;
  if(args.size() > 0) // argument provided
  {
    if(args[0] == "-" || args[0] == "/dev/stdin") //stdin
    {
      file = "/dev/stdin";
    }
    else
    {
      file=args[0];
    }
  }
  else
  {
    if(isatty(fileno(stdin))) // stdin is interactive
    {
      print_help(argv[0]);
      return 1;
    }
    else // is piped
    {
      file = "/dev/stdin";
      args.push_back("/dev/stdin");
    }
  }

  // parsing

  shmain* sh = new shmain(new list);
  shmain* tsh = nullptr;
  try
  {
    bool is_exec = false;
    bool first_run = true;

    // do parsing
    for(uint32_t i=0 ; i<args.size() ; i++)
    {
      std::string file = args[i];
      std::string filecontents=import_file(file);
      // parse
      g_origin=file;
      if(!add_include(file))
        continue;
      tsh = parse_text(filecontents, file);
      // resolve shebang
      if(first_run)
      {
        first_run=false;
        // resolve shebang
        bool shebang_is_bin = basename(argv[0]) == basename(tsh->shebang);
        if(shebang_is_bin)
          tsh->shebang="#!/bin/sh";

        // detect if need execution
        if(options['e'])
          is_exec=true;
        else if(options['c'] || options['o'])
          is_exec=false;
        else
          is_exec = shebang_is_bin;

        if(!is_exec && args.size() > 1) // not exec: parse options on args
        {
          args=options.process(args);
        }

        oneshot_opt_process(argv[0]);
        get_opts();
      }

      /* mid processing */
      // resolve/include
      if(g_include || g_resolve)
      {
        resolve(tsh);
      }

      // concatenate to main
      sh->concat(tsh);
      delete tsh;
      tsh = nullptr;

      // is exec: break and exec
      if(is_exec)
        break;
    }

    // processing before output
    if(options['m'])
      opt_minimize=true;
    if(options["minimize-var"])
      minimize_var( sh, re_var_exclude );
    if(options["minimize-fct"])
      minimize_fct( sh, re_fct_exclude );
    if(options["remove-unused"])
      delete_unused_fct( sh, re_fct_exclude );

    if(options["list-var"])
      list_vars(sh, re_var_exclude);
    else if(options["list-fct"])
      list_fcts(sh, re_var_exclude);
    else if(options["list-cmd"])
      list_cmds(sh, re_var_exclude);
    else if(is_exec)
    {
      ret = execute(sh, args);
    }
    else if(options['o']) // file output
    {
      std::string destfile=options['o'];
      // resolve - to stdout
      if(destfile == "-")
        destfile = "/dev/stdout";
      // output
      std::ofstream(destfile) << sh->generate();
      // don't chmod on /dev/
      if(destfile.substr(0,5) != "/dev/")
      ztd::exec("chmod", "+x", destfile);
    }
    else // to console
    {
      std::cout << sh->generate();
    }
  }
  catch(ztd::format_error& e)
  {
    if(tsh != nullptr)
      delete tsh;
    delete sh;
    printFormatError(e);
    return 100;
  }
  catch(std::runtime_error& e)
  {
    if(tsh != nullptr)
      delete tsh;
    delete sh;
    std::cerr << e.what() << std::endl;
    return 2;
  }

  delete sh;

  return ret;
}
