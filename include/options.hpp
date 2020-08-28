#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <ztd/options.hpp>

extern ztd::option_set options;

extern bool opt_minimize;

ztd::option_set gen_options();
void print_help(const char* arg0);

void print_include_help();
void print_resolve_help();

/**
%include [options]
options:
  -s   single quote contents
  -d   double quote contents
  -e   escape chars. For double quotes
  -r   include raw text, don't parse. Don't count as included
  -f   include even if already included. Don't count as included
*/
ztd::option_set create_include_opts();

/**
%resolve [options]
options:
  -e   escape chars. For double quotes
  -p   parse as shell code
  -f   ignore non zero return values
*/
ztd::option_set create_resolve_opts();

#endif //OPTIONS_HPP
