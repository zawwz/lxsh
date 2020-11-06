#ifndef RECURSIVE_HPP
#define RECURSIVE_HPP

#include <vector>

#include "struc.hpp"

// boolean value of fct: if true, recurse on this object, if false, skip this object
template<class... Args>
void recurse(void (&fct)(_obj*, Args...), _obj* o, Args... args)
{
  if(o == nullptr)
    return;

  // execution
  fct(o, args...);

  // recursive calls
  switch(o->type)
  {
    case _obj::_arg :
    {
      arg* t = dynamic_cast<arg*>(o);
      for(auto it: t->sa)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::_arglist :
    {
      arglist* t = dynamic_cast<arglist*>(o);
      for(auto it: t->args)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::_pipeline :
    {
      pipeline* t = dynamic_cast<pipeline*>(o);
      for(auto it: t->cmds)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::_condlist :
    {
      condlist* t = dynamic_cast<condlist*>(o);
      for(auto it: t->pls)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::_list :
    {
      list* t = dynamic_cast<list*>(o);
      for(auto it: t->cls)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::block_subshell :
    {
      subshell* t = dynamic_cast<subshell*>(o);
      recurse(fct, t->lst, args...);

      break;
    }
    case _obj::block_brace :
    {
      brace* t = dynamic_cast<brace*>(o);
      recurse(fct, t->lst, args...);
      break;
    }
    case _obj::block_main :
    {
      shmain* t = dynamic_cast<shmain*>(o);
      recurse(fct, t->lst, args...);

      break;
    }
    case _obj::block_function :
    {
      function* t = dynamic_cast<function*>(o);
      recurse(fct, t->lst, args...);
      break;
    }
    case _obj::block_cmd :
    {
      cmd* t = dynamic_cast<cmd*>(o);
      recurse(fct, t->args, args...);
      for(auto it: t->var_assigns)
        recurse(fct, it.second, args...);
      break;
    }
    case _obj::block_case :
    {
      case_block* t = dynamic_cast<case_block*>(o);
      // carg
      recurse(fct, t->carg, args...);
      // cases
      for(auto sc: t->cases)
      {
        for(auto it: sc.first)
        {
          recurse(fct, it, args...);
        }
        recurse(fct, sc.second, args...);
      }
      break;
    }
    case _obj::block_if :
    {
      if_block* t = dynamic_cast<if_block*>(o);
      // ifs
      for(auto sc: t->blocks)
      {
        // condition
        recurse(fct, sc.first, args...);
        // execution
        recurse(fct, sc.second, args...);
      }
      // else
      recurse(fct, t->else_lst, args...);
      break;
    }
    case _obj::block_for :
    {
      for_block* t = dynamic_cast<for_block*>(o);
      // iterations
      recurse(fct, t->iter, args...);
      // for block
      recurse(fct, t->ops, args...);
      break;
    }
    case _obj::block_while :
    {
      while_block* t = dynamic_cast<while_block*>(o);
      // condition
      recurse(fct, t->cond, args...);
      // operations
      recurse(fct, t->ops, args...);
      break;
    }
    case _obj::subarg_subshell :
    {
      subshell_subarg* t = dynamic_cast<subshell_subarg*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }

    default: break; //do nothing
  }
}


#endif //RECURSIVE_HPP
