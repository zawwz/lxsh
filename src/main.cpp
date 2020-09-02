#include <iostream>

#include <string.h>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include <unistd.h>

#include "util.hpp"
#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"

int execute(block& sh, std::vector<std::string>& args)
{
  std::string data=sh.generate();

  std::string filename=ztd::exec("basename", args[0]).first;
  filename.pop_back();

  // generate path
  std::string tmpdir = (getenv("TMPDIR") != NULL) ? getenv("TMPDIR") : "/tmp" ;
  std::string dirpath = tmpdir + "/lxsh_" + ztd::sh("tr -dc '[:alnum:]' < /dev/urandom | head -c10");
  std::string filepath = dirpath+'/'+filename;

  // create dir
  if(ztd::exec("mkdir", "-p", dirpath).second)
  {
    throw std::runtime_error("Failed to create directory '"+dirpath+'\'');
  }

  // create stream
  std::ofstream stream(filepath);
  if(!stream)
  {
    ztd::exec("rm", "-rf", dirpath);
    throw std::runtime_error("Failed to write to '"+filepath+'\'');
  }

  // output
  stream << data;
  stream.close();
  if(ztd::exec("chmod", "+x", filepath).second != 0)
  {
    ztd::exec("rm", "-rf", dirpath);
    throw std::runtime_error("Failed to make '"+filepath+"' executable");
  }

  // exec
  int retval=_exec(filepath, args);
  ztd::exec("rm", "-rf", dirpath);

  return retval;
}

int main(int argc, char* argv[])
{
  auto args=options.process(argc, argv, false, true);

  if(options['m'])
    opt_minimize=true;

  piped=false;

  if(options['h'])
  {
    print_help(argv[0]);
    return 1;
  }
  if(options["help-commands"])
  {
    print_include_help();
    printf("\n\n");
    print_resolve_help();
    return 1;
  }

  std::string file;
  if(args.size() > 0)
  {
    if(args[0] == "-")
    {
      piped=true;
      file = "/dev/stdin";
    }
    else
      file=args[0];
  }
  else
  {
    if(isatty(fileno(stdin)))
    {
      print_help(argv[0]);
      return 1;
    }
    else
    {
      piped=true;
      file = "/dev/stdin";
    }
  }


  g_origin=file;
  add_include(file);

  try
  {
    block sh(parse(import_file(file)));
    std::string curbin, binshebang;
    curbin=ztd::exec("basename", argv[0]).first;
    binshebang=ztd::exec("basename", sh.shebang).first;
    if(binshebang==curbin)
      sh.shebang="#!/bin/sh";
    if(options['e'])
    {
      return execute(sh, args);
    }
    else if(options['o'])
    {
      std::cout << sh.generate();
    }
    else
    {
      if(binshebang == curbin)
      {
        return execute(sh, args);
      }
      else
      {
        std::cout << sh.generate();
      }
    }
  }
  catch(ztd::format_error& e)
  {
    printFormatError(e);
    return 100;
  }
  catch(std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 2;
  }


  return 0;
}
