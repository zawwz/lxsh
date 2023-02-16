#include "profiling.hpp"

#include "struc.hpp"
#include "struc_helper.hpp"
#include "util.hpp"
#include "recursive.hpp"

bool r_insert_profiling(_obj* o) {
  switch(o->type)
  {
    case _obj::block_cmd: {
        cmd_t* c = dynamic_cast<cmd_t*>(o);
        if( ! c->is_cmdvar && !c->is_argvar() &&
            !is_in_vector( c->arg_string(0),  std::vector<std::string>({ "set", "shift", "echo", "printf", "[" }) ) &&
            c->args != nullptr
          ) {
          c->args->insert(0, make_arg("_lxsh_profile") ); 
        }
    };
    default: break;
  }
  return true;
}

void insert_profiling(_obj* o) {
  recurse(r_insert_profiling, o);
}
