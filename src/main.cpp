#include <iostream>

#include <string.h>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include <unistd.h>

#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"

int main(int argc, char* argv[])
{
  auto args=options.process(argc, argv);

  if(options['m'])
    opt_minimize=true;

  bool piped=false;

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

  if(!piped && !options['C'])
  {
    std::string dir=ztd::exec("dirname", file).first;
    dir.pop_back();
    file=ztd::exec("basename", file).first;
    file.pop_back();
    chdir(dir.c_str());
  }

  g_origin=file;
  add_include(file);

  if(args.size()>0)
  {
    try
    {
      block sh(parse(import_file(file)));
      std::cout << sh.generate();
    }
    catch(ztd::format_error& e)
    {
      printFormatException(e);
    }
    catch(std::exception& e)
    {
      std::cerr << e.what() << std::endl;
    }
  }

  return 0;
}
