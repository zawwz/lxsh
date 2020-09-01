#include <iostream>

#include <string.h>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include <unistd.h>

#include "util.hpp"
#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"

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
    if(options['e'])
    {
      std::string data=sh.generate();
      // generate path
      std::string tmpdir = (getenv("TMPDIR") != NULL) ? getenv("TMPDIR") : "/tmp" ;
      std::string filepath = tmpdir + "/lxsh_exec_" + ztd::sh("tr -dc '[:alnum:]' < /dev/urandom | head -c10");
      // create stream
      std::ofstream stream(filepath);
      if(!stream)
        throw std::runtime_error("Failed to write to file '"+filepath+'\'');

      // output
      stream << data;
      stream.close();
      auto p = ztd::exec("chmod", "+x", filepath);
      if(p.second != 0)
        return p.second;

      args.erase(args.begin());
      _exec(filepath, args);

    }
    else
    {
      std::cout << sh.generate();
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
