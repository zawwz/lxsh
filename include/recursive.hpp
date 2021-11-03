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
    case _obj::variable :
    {
      variable_t* t = dynamic_cast<variable_t*>(o);
      recurse(fct, t->index, args...);
      recurse(fct, t->manip, args...);
      break;
    }
    case _obj::redirect :
    {
      redirect_t* t = dynamic_cast<redirect_t*>(o);
      recurse(fct, t->target, args...);
      recurse(fct, t->here_document, args...);
      break;
    }
    case _obj::arg :
    {
      arg_t* t = dynamic_cast<arg_t*>(o);
      for(auto it: t->sa)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::arglist :
    {
      arglist_t* t = dynamic_cast<arglist_t*>(o);
      for(auto it: t->args)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::pipeline :
    {
      pipeline_t* t = dynamic_cast<pipeline_t*>(o);
      for(auto it: t->cmds)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::condlist :
    {
      condlist_t* t = dynamic_cast<condlist_t*>(o);
      for(auto it: t->pls)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::list :
    {
      list_t* t = dynamic_cast<list_t*>(o);
      for(auto it: t->cls)
      {
        recurse(fct, it, args...);
      }
      break;
    }
    case _obj::block_subshell :
    {
      subshell_t* t = dynamic_cast<subshell_t*>(o);
      recurse(fct, t->lst, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_brace :
    {
      brace_t* t = dynamic_cast<brace_t*>(o);
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
      function_t* t = dynamic_cast<function_t*>(o);
      recurse(fct, t->lst, args...);

      for(auto it: t->redirs)
        recurse(fct, it, args...);

      break;
    }
    case _obj::block_cmd :
    {
      cmd_t* t = dynamic_cast<cmd_t*>(o);
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
      case_t* t = dynamic_cast<case_t*>(o);
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
      if_t* t = dynamic_cast<if_t*>(o);
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
      for_t* t = dynamic_cast<for_t*>(o);
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
      while_t* t = dynamic_cast<while_t*>(o);
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
      subarg_variable_t* t = dynamic_cast<subarg_variable_t*>(o);
      recurse(fct, t->var, args...);
      break;
    }
    case _obj::subarg_subshell :
    {
      subarg_subshell_t* t = dynamic_cast<subarg_subshell_t*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::subarg_procsub :
    {
      subarg_procsub_t* t = dynamic_cast<subarg_procsub_t*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::subarg_arithmetic :
    {
      subarg_arithmetic_t* t = dynamic_cast<subarg_arithmetic_t*>(o);
      recurse(fct, t->arith, args...);
      break;
    }
    case _obj::arithmetic_variable :
    {
      arithmetic_variable_t* t = dynamic_cast<arithmetic_variable_t*>(o);
      recurse(fct, t->var, args...);
      break;
    }
    case _obj::arithmetic_subshell :
    {
      arithmetic_subshell_t* t = dynamic_cast<arithmetic_subshell_t*>(o);
      recurse(fct, t->sbsh, args...);
      break;
    }
    case _obj::arithmetic_operation :
    {
      arithmetic_operation_t* t = dynamic_cast<arithmetic_operation_t*>(o);
      recurse(fct, t->val1, args...);
      recurse(fct, t->val2, args...);
      break;
    }
    case _obj::arithmetic_parenthesis :
    {
      arithmetic_parenthesis_t* t = dynamic_cast<arithmetic_parenthesis_t*>(o);
      recurse(fct, t->val, args...);
      break;
    }

    default: break; //do nothing
  }
}


#endif //RECURSIVE_HPP
