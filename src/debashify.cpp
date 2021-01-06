#include "debashify.hpp"

#include "recursive.hpp"

bool debashify_replace_bashtest(cmd* in)
{
  if(in->firstarg_string() == "[[")
    throw std::runtime_error("Debashify on '[[' not implemented yet");

  return false;
}

bool debashify_replace_combined_redirects(block* in)
{
  bool has_replaced=false;


  for(uint32_t i=0; i<in->redirs.size() ; i++)
  {
    if(in->redirs[i]->op == "&>" || in->redirs[i]->op == "&>>" || in->redirs[i]->op == ">&")
    {
      // resolve new operator
      std::string newop = ">";
      if( in->redirs[i]->op == "&>>" )
        newop = ">>";
      // create new redir with target
      redirect* newredir = new redirect(newop, in->redirs[i]->target);
      in->redirs[i]->target=nullptr;
      // replace old redir
      delete in->redirs[i];
      in->redirs[i] = newredir;
      // insert merge redir
      i++;
      in->redirs.insert(in->redirs.begin()+i, new redirect("2>&1"));

      has_replaced=true;
    }
  }

  return has_replaced;
}

bool r_debashify(_obj* o)
{
  switch(o->type)
  {
    case _obj::block_subshell: {
      subshell* t = dynamic_cast<subshell*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_brace: {
      brace* t = dynamic_cast<brace*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_main: {
      shmain* t = dynamic_cast<shmain*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_cmd: {
      cmd* t = dynamic_cast<cmd*>(o);
      debashify_replace_combined_redirects(t);
      debashify_replace_bashtest(t);
    } break;
    case _obj::block_function: {
      function* t = dynamic_cast<function*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_case: {
      case_block* t = dynamic_cast<case_block*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_if: {
      if_block* t = dynamic_cast<if_block*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_while: {
      while_block* t = dynamic_cast<while_block*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    case _obj::block_for: {
      for_block* t = dynamic_cast<for_block*>(o);
      debashify_replace_combined_redirects(t);
    } break;
    default: break;
  }
  return true;
}

void debashify(shmain* sh)
{
  recurse(r_debashify, sh);
}
