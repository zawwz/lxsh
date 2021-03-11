#include "options.hpp"

#include "processing.hpp"

ztd::option_set options = gen_options();
bool opt_minimize=false;

bool g_cd=false;
bool g_include=true;
bool g_resolve=true;
bool g_shebang=true;

ztd::option_set gen_options()
{
  ztd::option_set ret;
  ret.add(
      ztd::option("\r  [Help]"),
      ztd::option('h', "help",          false, "Display this help message"),
      ztd::option("version",            false, "Display version"),
      ztd::option("help-commands",      false, "Print help for linker commands"),
      ztd::option("\r  [Output]"),
      ztd::option('o', "output",        true , "Output result script to file", "file"),
      ztd::option('c', "stdout",        false, "Output result script to stdout"),
      ztd::option('e', "exec",          false, "Directly execute script"),
      ztd::option("no-shebang",         false, "Don't output shebang"),
      ztd::option('J', "json",          false, "Output the json structure"),
      ztd::option("\r  [Processing]"),
      ztd::option('m', "minimize",      false, "Minimize code without changing functionality"),
      ztd::option("minimize-quotes",    false, "Remove unnecessary quotes"),
      ztd::option('C', "no-cd",         false, "Don't cd when doing %include and %resolve"),
      ztd::option('I', "no-include",    false, "Don't resolve %include commands"),
      ztd::option('R', "no-resolve",    false, "Don't resolve %resolve commands"),
      ztd::option("debashify",          false, "Attempt to turn a bash-specific script into a POSIX shell script"),
      ztd::option("\r  [var/fct processing]"),
      ztd::option("minimize-var",       false, "Minimize variable names"),
      ztd::option("minimize-fct",       false, "Minimize function names"),
      ztd::option("exclude-var",        true,  "List of matching regex to ignore for variable processing", "list"),
      ztd::option("exclude-fct",        true,  "List of matching regex to ignore for function processing", "list"),
      ztd::option("no-exclude-reserved",false, "Don't exclude reserved variables"),
      ztd::option("list-var",           false, "List all variables set and invoked in the script"),
      ztd::option("list-var-def",       false, "List all variables set in the script"),
      ztd::option("list-var-call",      false, "List all variables invoked in the script"),
      ztd::option("list-fct",           false, "List all functions defined in the script"),
      ztd::option("list-cmd",           false, "List all commands invoked in the script"),
      ztd::option("remove-unused",      false, "Remove unused functions and variables"),
      ztd::option("unset-var",          false, "Add 'unset' to all vars at the start of the script to avoid environment interference")
  );
  return ret;
}

void get_opts()
{
  g_cd=!options['C'].activated;
  g_include=!options["no-include"].activated;
  g_resolve=!options["no-resolve"].activated;
  g_shebang=!options["no-shebang"].activated;
  if(options["exclude-var"])
    re_var_exclude=var_exclude_regex(options["exclude-var"], !options["no-exclude-reserved"]);
  else
    re_var_exclude=var_exclude_regex("", !options["no-exclude-reserved"]);
  if(options["exclude-fct"])
    re_fct_exclude=fct_exclude_regex(options["exclude-fct"]);
}

ztd::option_set create_include_opts()
{
  ztd::option_set opts;
  opts.add(
    ztd::option('e', false, "Escape double quotes"),
    ztd::option('C', false, "Don't cd to folder the file is in"),
    ztd::option('f', false, "Force include even if already included. Don't count as included")
  );
  return opts;
}

ztd::option_set create_resolve_opts()
{
  ztd::option_set opts;
  opts.add(
    ztd::option('C', false, "Don't cd to folder this file is in"),
    ztd::option('f', false, "Ignore non-zero return values")
  );
  return opts;
}

void print_help(const char* arg0)
{
  printf("%s [options] <file> [arg...]\n", arg0);
  printf("Link extended shell\n");
  printf("Include files and resolve commands on build time\n");
  printf("See --help-commands for help on linker commands\n");
  printf("\n");
  printf("Options:\n");
  options.print_help(4,25);
}

void print_include_help()
{
  printf("%%include [options] <file...>\n");
  printf("Include the targeted files, from folder of current file\n");
  printf("Default behaviour is to include and parse contents as shell code\n");
  printf(" - Regular shell processing applies to the file arguments\n");
  printf(" - Only includes not already included files. Can be forced with -f\n");
  printf(" - `%%include` inside substitutions replaces the substitution and includes raw contents\n");
  printf("\n");

  ztd::option_set opts=create_include_opts();
  printf("Options:\n");
  opts.print_help(3,7);
}
void print_resolve_help()
{
  printf("%%resolve [options] <command...>\n");
  printf("Execute shell command and substitute output, from folder of current file\n");
  printf(" - Default behaviour is to parse contents as shell code\n");
  printf(" - Fails if return value is not 0. Can be ignored with -f\n");
  printf(" - `%%include` inside substitutions replaces the substitution and puts raw response\n");
  printf("\n");

  ztd::option_set opts=create_resolve_opts();
  printf("Options:\n");
  opts.print_help(3,7);
}
