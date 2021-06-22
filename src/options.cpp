#include "options.hpp"

#include "processing.hpp"
#include "shellcode.hpp"

#include "errcodes.h"
#include "version.h"
#include "g_version.h"

bool opt_minify=false;

ztd::option_set options( {
  ztd::option("\r  [Help]"),
  ztd::option('h', "help",          false, "Display this help message"),
  ztd::option("version",            false, "Display version"),
  ztd::option("help-link-commands", false, "Print help for linker commands"),
  ztd::option("help-extend-fcts",   false, "Print help for lxsh extension functions"),
  ztd::option("\r  [Output]"),
  ztd::option('o', "output",        true , "Output result script to file", "file"),
  ztd::option('c', "stdout",        false, "Output result script to stdout"),
  ztd::option('e', "exec",          false, "Directly execute script"),
  ztd::option("no-shebang",         false, "Don't output shebang"),
#ifdef DEBUG_MODE
  ztd::option("\r  [Debugging]"),
  ztd::option('J', "json",          false, "Output the json structure"),
#endif
  ztd::option("\r  [Processing]"),
  ztd::option('m', "minify",        false, "Minify code without changing functionality"),
  ztd::option('M', "minify-full",   false, "Enable all minifying features: -m --minify-quotes --minify-var --minify-fct --remove-unused"),
  ztd::option("minify-quotes",      false, "Remove unnecessary quotes"),
  ztd::option('C', "no-cd",         false, "Don't cd when doing %include and %resolve"),
  ztd::option('I', "no-include",    false, "Don't resolve %include commands"),
  ztd::option('R', "no-resolve",    false, "Don't resolve %resolve commands"),
  ztd::option("no-extend",          false, "Don't add lxsh extension functions"),
  ztd::option("bash",               false, "Force bash parsing"),
  ztd::option("debashify",          false, "Attempt to turn a bash-specific script into a POSIX shell script"),
  ztd::option("remove-unused",      false, "Remove unused functions and variables"),
  ztd::option("list-cmd",           false, "List all commands invoked in the script"),
  ztd::option("\r  [Variable processing]"),
  ztd::option("exclude-var",        true,  "List of matching regex to ignore for variable processing", "list"),
  ztd::option("no-exclude-reserved",false, "Don't exclude reserved variables"),
  ztd::option("minify-var",         false, "Minify variable names"),
  ztd::option("list-var",           false, "List all variables set and invoked in the script"),
  ztd::option("list-var-def",       false, "List all variables set in the script"),
  ztd::option("list-var-call",      false, "List all variables invoked in the script"),
  ztd::option("unset-var",          false, "Add 'unset' to all variables at the start of the script to avoid environment interference"),
  ztd::option("\r  [Function processing]"),
  ztd::option("exclude-fct",        true,  "List of matching regex to ignore for function processing", "list"),
  ztd::option("minify-fct",         false, "Minify function names"),
  ztd::option("list-fct",           false, "List all functions defined in the script")
} );

bool g_cd=false;
bool g_include=true;
bool g_resolve=true;
bool g_shebang=true;

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
  if(options['M'])
  {
    options['m'].activated=true;
    options["minify-var"].activated=true;
    options["minify-fct"].activated=true;
    options["minify-quotes"].activated=true;
    options["remove-unused"].activated=true;
  }
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
  printf("Extended shell linker\n");
  printf("Include files, resolve commands on build time, process and minify shell code\n");
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

void print_lxsh_extension_help()
{
  for(auto it: lxsh_extend_fcts)
  {
    printf("%s %s\n%s\n\n", it.first.c_str(), it.second.arguments.c_str(), it.second.description.c_str());
  }
}

void oneshot_opt_process(const char* arg0)
{
  if(options['h'])
  {
    print_help(arg0);
    exit(ERR_HELP);
  }
  else if(options["version"])
  {
    printf("%s %s%s\n", arg0, VERSION_STRING, VERSION_SUFFIX);
    printf("%s\n", VERSION_SHA);
    exit(0);
  }
  else if(options["help-link-commands"])
  {
    print_include_help();
    printf("\n\n");
    print_resolve_help();
    exit(ERR_HELP);
  }
  else if(options["help-extend-fcts"])
  {
    print_lxsh_extension_help();
    exit(ERR_HELP);
  }
}
