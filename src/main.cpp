#include <iostream>

#include <string.h>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include <unistd.h>

#include "util.hpp"
#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"

int execute(shmain* sh, std::vector<std::string>& args)
{
  std::string data=sh->generate();

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


  // resolve input
  std::string file;
  if(args.size() > 0) // argument provided
  {
    if(args[0] == "-" || args[0] == "/dev/stdin") //stdin
    {
      piped=true;
      file = "/dev/stdin";
    }
    else
      file=args[0];
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
      piped=true;
      file = "/dev/stdin";
      args.push_back("/dev/stdin");
    }
  }

  // set origin file
  g_origin=file;
  add_include(file);

  shmain* sh=nullptr;
  try
  {
    // parse
    sh = parse(import_file(file));
    // resolve shebang
    std::string curbin, binshebang;
    curbin=ztd::exec("basename", argv[0]).first;
    binshebang=ztd::exec("basename", sh->shebang).first;
    if(binshebang==curbin)
      sh->shebang="#!/bin/sh";
    // process
    if(options['e']) // force exec
    {
      return execute(sh, args);
    }
    else if(options['c']) // force console out
    {
      std::cout << sh->generate();
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
    else // other process
    {
      if(binshebang == curbin) // exec if shebang is program
      {
        return execute(sh, args);
      }
      else // output otherwise
      {
        std::cout << sh->generate();
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

  if(sh!=nullptr)
    delete sh;


  return 0;
}
