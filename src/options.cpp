#include "options.hpp"

ztd::option_set options = gen_options();
bool opt_minimize;

ztd::option_set gen_options()
{
  ztd::option_set ret;
  ret.add(ztd::option('h', "help",     false, "Display this help message"));
  ret.add(ztd::option('m', "minimize", false, "Minimize code"));
  ret.add(ztd::option('C', "no-cd",    false, "Don't change directories"));
  ret.add(ztd::option("help-commands", false, "Print help for linker commands"));
  return ret;
}

ztd::option_set create_include_opts()
{
  ztd::option_set opts;
  opts.add(
    ztd::option('s', false, "Single quote contents"),
    ztd::option('d', false, "Double quote contents"),
    // ztd::option('e', false, "Escape for double quotes"),
    ztd::option('r', false, "Include raw contents, don't parse"),
    ztd::option('f', false, "Force include even if already included. Don't count as included")
  );
  return opts;
}

ztd::option_set create_resolve_opts()
{
  ztd::option_set opts;
  opts.add(
    // ztd::option('e', false, "Escape for double quotes"),
    ztd::option('p', false, "Parse contents as shell code"),
    ztd::option('f', false, "Ignore non-zero return values")
  );
  return opts;
}

void print_help(const char* arg0)
{
  printf("%s [options] [file]\n", arg0);
  printf("Link extended shell, allows file including and command resolving\n");
  printf("See --help-commands for help on linker commands\n");
  printf("\n");
  printf("Options:\n");
  options.print_help(3,20);
}

void print_include_help()
{
  printf("%%include [options] <file...>\n");
  printf("Include the targeted files. Paths are relative to folder of current file\n");
  printf(" - Regular shell processing applies to the file arguments\n");
  printf(" - Only includes not already included files\n");
  printf(" - `%%include` in command substitutions replaces the substitution\n");
  printf(" =>`%%include_s` can be used inside a substitution to prevent this\n");
  printf("\n");

  ztd::option_set opts=create_include_opts();
  printf("Options:\n");
  opts.print_help(3,7);
}
void print_resolve_help()
{
  printf("%%resolve [options] <command...>\n");
  printf("Execute shell command and substitute output. Paths is from folder of current file\n");
  printf(" - Fails if return value is not 0. Can be ignored with -f\n");
  printf(" - `%%resolve` in command substitutions replaces the substitution\n");
  printf(" =>`%%resolve_s` can be used inside a substitution to prevent this\n");
  printf("\n");

  ztd::option_set opts=create_resolve_opts();
  printf("Options:\n");
  opts.print_help(3,7);
}
