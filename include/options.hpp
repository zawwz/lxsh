#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <ztd/options.hpp>

extern ztd::option_set options;

extern bool opt_minify;
extern bool g_cd;
extern bool g_include;
extern bool g_resolve;
extern bool g_shebang;

void print_lxsh_extension_help();

void get_opts();

ztd::option_set gen_options();
void print_help(const char* arg0);

void print_include_help();
void print_resolve_help();

/**
%include [options]
options:
  -C   Don't cd
  -e   escape chars. For double quotes
  -f   include even if already included. Don't count as included
*/
ztd::option_set create_include_opts();

/**
%resolve [options]
options:
  -C   Don't cd
  -e   escape chars. For double quotes
  -f   ignore non zero return values
*/
ztd::option_set create_resolve_opts();

void oneshot_opt_process(const char* arg0);

#endif //OPTIONS_HPP
