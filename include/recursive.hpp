#ifndef RECURSIVE_HPP
#define RECURSIVE_HPP

#include <vector>

#include "struc.hpp"

// boolean value of fct: if true, recurse on this object, if false, skip this object
template<class... Args>
void recurse(bool (&fct)(_obj*, Args...), _obj* o, Args... args)
{
  if(o == nullptr)
    return;

  // execution
  if(!fct(o, args...))
    return; // skip recurse if false

  // recursive calls
  switch(o->type)
  {
    case _obj::_variable :
    {
      variable* t = dynamic_cast<variable*>(o);
      recurse(fct, t->index, args...);
      recurse(fct, t->manip, args...);
      break;
    }
    case _obj::_redirect :
    {
      redirect* t = dynamic_cast<redirect*>(o);
      recurse(fct, t->target, args...);
      break;
    }
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

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_brace :
    {
      brace* t = dynamic_cast<brace*>(o);
      recurse(fct, t->lst, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_main :
    {
      shmain* t = dynamic_cast<shmain*>(o);
      recurse(fct, t->lst, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_function :
    {
      function* t = dynamic_cast<function*>(o);
      recurse(fct, t->lst, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_cmd :
    {
      cmd* t = dynamic_cast<cmd*>(o);
      recurse(fct, t->args, args...);
      for(auto it: t->var_assigns)
      {
        recurse(fct, it.first, args...);
        recurse(fct, it.second, args...);
      }
      for(auto it: t->cmd_var_assigns)
      {
        recurse(fct, it.first, args...);
        recurse(fct, it.second, args...);
      }

      for(auto it: t->redirs)
        recurse(fct, it, args...);

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

      for(auto it: t->redirs)
        recurse(fct, it, args...);

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

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_for :
    {
      for_block* t = dynamic_cast<for_block*>(o);
      // variable
      recurse(fct, t->var, args...);
      // iterations
      recurse(fct, t->iter, args...);
      // for block
      recurse(fct, t->ops, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_while :
    {
      while_block* t = dynamic_cast<while_block*>(o);
      // condition
      recurse(fct, t->cond, args...);
      // operations
      recurse(fct, t->ops, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::subarg_variable :
    {
      variable_subarg* t = dynamic_cast<variable_subarg*>(o);
      recurse(fct, t->var, args...);
      break;
    }
    case _obj::subarg_subshell :
    {
      subshell_subarg* t = dynamic_cast<subshell_subarg*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::subarg_procsub :
    {
      procsub_subarg* t = dynamic_cast<procsub_subarg*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::subarg_arithmetic :
    {
      arithmetic_subarg* t = dynamic_cast<arithmetic_subarg*>(o);
      recurse(fct, t->arith, args...);
      break;
    }
    case _obj::arithmetic_variable :
    {
      variable_arithmetic* t = dynamic_cast<variable_arithmetic*>(o);
      recurse(fct, t->var, args...);
      break;
    }
    case _obj::arithmetic_subshell :
    {
      subshell_arithmetic* t = dynamic_cast<subshell_arithmetic*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::arithmetic_operation :
    {
      operation_arithmetic* t = dynamic_cast<operation_arithmetic*>(o);
      recurse(fct, t->val1, args...);
      recurse(fct, t->val2, args...);
      break;
    }
    case _obj::arithmetic_parenthesis :
    {
      parenthesis_arithmetic* t = dynamic_cast<parenthesis_arithmetic*>(o);
      recurse(fct, t->val, args...);
      break;
    }

    default: break; //do nothing
  }
}


#endif //RECURSIVE_HPP
