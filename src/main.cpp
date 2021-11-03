#include <iostream>

#include <string.h>
#include <unistd.h>

#include <fstream>

#include <ztd/options.hpp>
#include <ztd/shell.hpp>

#include "util.hpp"
#include "struc.hpp"
#include "parse.hpp"
#include "options.hpp"
#include "recursive.hpp"
#include "minify.hpp"
#include "resolve.hpp"
#include "processing.hpp"
#include "debashify.hpp"
#include "exec.hpp"
#include "shellcode.hpp"

#include "errcodes.h"

int main(int argc, char* argv[])
{
  std::vector<std::string> args;

  int ret=0;

  bool optstop=false;

  shmain *sh=nullptr, *tsh=nullptr;
  try
  {
    args=options.process(argc, argv, {.stop_on_argument=true, .output_doubledash=true} );
    if( args.size()>0 && args[0] == "--" )
    {
      optstop=true;
      args.erase(args.begin());
    }


    oneshot_opt_process(argv[0]);

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
        return ERR_HELP;
      }
      else // is piped
      {
        file = "/dev/stdin";
        args.push_back("/dev/stdin");
      }
    }

    // parsing

    sh = new shmain;

    bool is_exec = false;
    bool first_run = true;

    // do parsing
    bool shebang_is_bin=false;
    bool parse_bash=false;
    parse_context ctx;
    std::string binshebang;
    for(uint32_t i=0 ; i<args.size() ; i++)
    {
      std::string file = args[i];
      std::string filecontents=import_file(file);
      std::string shebang=filecontents.substr(0,filecontents.find('\n'));
      if(shebang.substr(0,2) != "#!")
        shebang="#!/bin/sh";
      // resolve shebang and parse leftover options
      if(first_run)
      {
        first_run=false;
        // resolve shebang
        if(options["bash"])
        {
          parse_bash=true;
          shebang = "#!/usr/bin/env bash";
        }
        else
        {
          binshebang = basename(shebang);
          shebang_is_bin = ( basename(argv[0]) == binshebang );
          parse_bash = (options["debashify"] || binshebang == "bash" || binshebang == "lxsh");
        }

        // detect if need execution
        if(options['e'])
          is_exec=true;
        else if(options['c'] || options['o'])
          is_exec=false;
        else
          is_exec = shebang_is_bin;

        if(!is_exec && args.size() > 1 && !optstop) // not exec: parse options on args
          args=options.process(args);

        if(!is_exec && options['e'])
          throw std::runtime_error("Option -e must be before file");

        if(shebang_is_bin) // enable debashify option
        {
          shebang="#!/bin/sh";
          options["debashify"].activated=true;
        }

        oneshot_opt_process(argv[0]);
        get_opts();

      }
      // parse
      if(!add_include(file))
        continue;

      ctx.data=filecontents.data();

      ctx = make_context(filecontents, file, parse_bash);
      if(is_exec)
      {
        args.erase(args.begin());
        return exec_process(shebang.substr(2), args, ctx);
      }
      else
      {
        auto pp = parse_text(ctx);
        tsh = pp.first;
        if(options["bash"])
          tsh->shebang = "#!/usr/bin/env bash";
        ctx = pp.second;
        if(shebang_is_bin) // resolve lxsh shebang to sh
          tsh->shebang="#!/bin/sh";

        /* mid processing */
        // resolve/include
        if(g_include || g_resolve)
          resolve(tsh, ctx);

        // concatenate to main
        sh->concat(tsh);
        delete tsh;
        tsh = nullptr;
      }
    } // end of argument parse

    // pre-listing modifiers
    if(options["remove-unused"])
      delete_unused( sh, re_var_exclude, re_fct_exclude );

    // list outputs
    if(options["list-var"])
      list_vars(sh, re_var_exclude);
    else if(options["list-var-def"])
      list_var_defs(sh, re_var_exclude);
    else if(options["list-var-call"])
      list_var_calls(sh, re_var_exclude);
    else if(options["list-fct"])
      list_fcts(sh, re_fct_exclude);
    else if(options["list-cmd"])
      list_cmds(sh, regex_null);
    // output
    else
    {
      // post-listing modifiers
      // implement commands
      std::set<std::string> req_fcts;
      if(shebang_is_bin && !options["no-extend"])
        req_fcts = find_lxsh_commands(sh);
      if(options["debashify"])
        concat_sets(req_fcts, debashify(sh) );

      add_lxsh_fcts(sh, req_fcts);

      // processing before output
      // minify
      strmap_t varmap, fctmap;
      if(options['m'])
      {
        opt_minify=true;
        minify_generic(sh);
      }
      if(options['A']) {
        read_minmap(options['A'].argument, &varmap, &fctmap);
        recurse(r_replace_var, sh, &varmap);
        recurse(r_replace_fct, sh, &fctmap);
      }
      else if(options["minify-var"] && options["minify-fct"]) {
        // optimization: get everything in one go
        allmaps_get(sh, re_var_exclude, re_fct_exclude, regex_null);
        varmap = minify_var( sh, re_var_exclude );
        fctmap = minify_fct( sh, re_fct_exclude );
      }
      else if(options["minify-var"]) {
        varmap = minify_var( sh, re_var_exclude );
      }
      else if(options["minify-fct"]) {
        fctmap = minify_fct( sh, re_fct_exclude );
      }
      // other processing
      if(options["unset-var"])
        add_unset_variables( sh, re_var_exclude );

  #ifdef DEBUG_MODE
      if(options['J'])
      {
        std::cout << gen_json_struc(sh) << std::endl;
      }
      else
  #endif

      if(options['P']) {
        std::ofstream(options['P'].argument) << gen_minmap(varmap, "var") << gen_minmap(fctmap, "fct");
      }

      if(options['o']) // file output
      {
        std::string destfile=options['o'];
        // resolve - to stdout
        if(destfile == "-")
          destfile = "/dev/stdout";
        // output
        std::ofstream(destfile) << sh->generate(g_shebang, 0);
        // don't chmod on /dev/
        if(destfile.substr(0,5) != "/dev/")
          ztd::exec("chmod", "+x", destfile);
      }
      else // to console
      {
        std::cout << sh->generate(g_shebang, 0);
      }
    }
  }
  catch(format_error& e)
  {
    if(tsh != nullptr)
      delete tsh;
    delete sh;
    printFormatError(e);
    return ERR_PARSE;
  }
  catch(ztd::option_error& e)
  {
    std::cerr << e.what() << std::endl;
    return ERR_OPT;
  }
  catch(std::runtime_error& e)
  {
    if(tsh != nullptr)
      delete tsh;
    if(sh != nullptr)
      delete sh;
    std::cerr << e.what() << std::endl;
    return ERR_RUNTIME;
  }
  delete sh;

  return ret;
}
