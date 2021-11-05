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
        recurse(fct, it, args...);
      break;
    }
    case _obj::arglist :
    {
      arglist_t* t = dynamic_cast<arglist_t*>(o);
      for(auto it: t->args)
        recurse(fct, it, args...);
      break;
    }
    case _obj::pipeline :
    {
      pipeline_t* t = dynamic_cast<pipeline_t*>(o);
      for(auto it: t->cmds)
        recurse(fct, it, args...);
      break;
    }
    case _obj::condlist :
    {
      condlist_t* t = dynamic_cast<condlist_t*>(o);
      for(auto it: t->pls)
        recurse(fct, it, args...);
      break;
    }
    case _obj::list :
    {
      list_t* t = dynamic_cast<list_t*>(o);
      for(auto it: t->cls)
        recurse(fct, it, args...);
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

// deep copy of object structure
template<class... Args>
_obj* obj_copy(_obj* o)
{
  if(o == nullptr)
    return nullptr;

  // recursive calls
  switch(o->type)
  {
    case _obj::variable :
    {
      variable_t* t = dynamic_cast<variable_t*>(o);
      variable_t* ret = new variable_t(*t);

      ret->index = dynamic_cast<arg_t*>(obj_copy(t->index));
      ret->manip = dynamic_cast<arg_t*>(obj_copy(t->manip));
      return ret;
    }
    case _obj::redirect :
    {
      redirect_t* t = dynamic_cast<redirect_t*>(o);
      redirect_t* ret = new redirect_t(*t);

      ret->target = dynamic_cast<arg_t*>(obj_copy(t->target));
      ret->here_document = dynamic_cast<arg_t*>(obj_copy(t->here_document));
      return ret;
    }
    case _obj::arg :
    {
      arg_t* t = dynamic_cast<arg_t*>(o);
      arg_t* ret = new arg_t(*t);

      for(uint32_t i=0; i<ret->sa.size(); i++)
        ret->sa[i] = dynamic_cast<subarg_t*>(obj_copy(t->sa[i]));
      return ret;
    }
    case _obj::arglist :
    {
      arglist_t* t = dynamic_cast<arglist_t*>(o);
      arglist_t* ret = new arglist_t(*t);

      for(uint32_t i=0; i<ret->args.size(); i++)
        ret->args[i] = dynamic_cast<arg_t*>(obj_copy(t->args[i]));
      return ret;
    }
    case _obj::pipeline :
    {
      pipeline_t* t = dynamic_cast<pipeline_t*>(o);
      pipeline_t* ret = new pipeline_t(*t);

      for(uint32_t i=0; i<ret->cmds.size(); i++)
        ret->cmds[i] = dynamic_cast<block_t*>(obj_copy(t->cmds[i]));
      return ret;
    }
    case _obj::condlist :
    {
      condlist_t* t = dynamic_cast<condlist_t*>(o);
      condlist_t* ret = new condlist_t(*t);

      for(uint32_t i=0; i<ret->pls.size(); i++)
        ret->pls[i] = dynamic_cast<pipeline_t*>(obj_copy(t->pls[i]));
      return ret;
    }
    case _obj::list :
    {
      list_t* t = dynamic_cast<list_t*>(o);
      list_t* ret = new list_t(*t);

      for(uint32_t i=0; i<ret->cls.size(); i++)
        ret->cls[i] = dynamic_cast<condlist_t*>(obj_copy(t->cls[i]));
      return ret;
    }
    case _obj::block_subshell :
    {
      subshell_t* t = dynamic_cast<subshell_t*>(o);
      subshell_t* ret = new subshell_t(*t);

      ret->lst = dynamic_cast<list_t*>(obj_copy(t->lst));
      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_brace :
    {
      brace_t* t = dynamic_cast<brace_t*>(o);
      brace_t* ret = new brace_t(*t);

      ret->lst = dynamic_cast<list_t*>(obj_copy(t->lst));
      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_main :
    {
      shmain* t = dynamic_cast<shmain*>(o);
      shmain* ret = new shmain(*t);

      ret->lst = dynamic_cast<list_t*>(obj_copy(t->lst));
      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_function :
    {
      function_t* t = dynamic_cast<function_t*>(o);
      function_t* ret = new function_t(*t);

      ret->lst = dynamic_cast<list_t*>(obj_copy(t->lst));
      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_cmd :
    {
      cmd_t* t = dynamic_cast<cmd_t*>(o);
      cmd_t* ret = new cmd_t(*t);

      ret->args = dynamic_cast<arglist_t*>(obj_copy(t->args));

      for(uint32_t i=0; i<ret->var_assigns.size(); i++)
      {
        ret->var_assigns[i].first = dynamic_cast<variable_t*>(obj_copy(t->var_assigns[i].first));
        ret->var_assigns[i].second = dynamic_cast<arg_t*>(obj_copy(t->var_assigns[i].second));
      }
      for(uint32_t i=0; i<ret->cmd_var_assigns.size(); i++)
      {
        ret->cmd_var_assigns[i].first = dynamic_cast<variable_t*>(obj_copy(t->cmd_var_assigns[i].first));
        ret->cmd_var_assigns[i].second = dynamic_cast<arg_t*>(obj_copy(t->cmd_var_assigns[i].second));
      }

      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_case :
    {
      case_t* t = dynamic_cast<case_t*>(o);
      case_t* ret = new case_t(*t);

      // carg
      ret->carg = dynamic_cast<arg_t*>(obj_copy(t->carg));
      // cases
      for(uint32_t i=0; i<ret->cases.size(); i++)
      {
        for(uint32_t j=0; j<ret->cases[i].first.size(); i++)
          ret->cases[i].first[j] = dynamic_cast<arg_t*>(obj_copy(t->cases[i].first[j]));
        ret->cases[i].second = dynamic_cast<list_t*>(obj_copy(t->cases[i].second));
      }

      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_if :
    {
      if_t* t = dynamic_cast<if_t*>(o);
      if_t* ret = new if_t(*t);

      // ifs
      for(uint32_t i=0; i<ret->blocks.size(); i++)
      {
        // condition
        ret->blocks[i].first = dynamic_cast<list_t*>(obj_copy(t->blocks[i].first));
        // execution
        ret->blocks[i].second = dynamic_cast<list_t*>(obj_copy(t->blocks[i].second));
      }
      // else
      ret->else_lst = dynamic_cast<list_t*>(obj_copy(t->else_lst));

      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_for :
    {
      for_t* t = dynamic_cast<for_t*>(o);
      for_t* ret = new for_t(*t);

      // variable
      ret->var = dynamic_cast<variable_t*>(obj_copy(t->var));
      // iterations
      ret->iter = dynamic_cast<arglist_t*>(obj_copy(t->iter));
      // for block
      ret->ops = dynamic_cast<list_t*>(obj_copy(t->ops));

      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::block_while :
    {
      while_t* t = dynamic_cast<while_t*>(o);
      while_t* ret = new while_t(*t);

      // condition
      ret->cond = dynamic_cast<list_t*>(obj_copy(t->cond));
      // for operations
      ret->ops = dynamic_cast<list_t*>(obj_copy(t->ops));

      for(uint32_t i=0; i<ret->redirs.size(); i++)
        ret->redirs[i] = dynamic_cast<redirect_t*>(obj_copy(t->redirs[i]));

      return ret;
    }
    case _obj::subarg_string :
    {
      subarg_string_t* t = dynamic_cast<subarg_string_t*>(o);
      subarg_string_t* ret = new subarg_string_t(*t);
      return ret;
    }
    case _obj::subarg_variable :
    {
      subarg_variable_t* t = dynamic_cast<subarg_variable_t*>(o);
      subarg_variable_t* ret = new subarg_variable_t(*t);
      ret->var = dynamic_cast<variable_t*>(obj_copy(t->var));
      return ret;
    }
    case _obj::subarg_subshell :
    {
      subarg_subshell_t* t = dynamic_cast<subarg_subshell_t*>(o);
      subarg_subshell_t* ret = new subarg_subshell_t(*t);
      ret->sbsh = dynamic_cast<subshell_t*>(obj_copy(t->sbsh));
      return ret;
    }
    case _obj::subarg_procsub :
    {
      subarg_procsub_t* t = dynamic_cast<subarg_procsub_t*>(o);
      subarg_procsub_t* ret = new subarg_procsub_t(*t);
      ret->sbsh = dynamic_cast<subshell_t*>(obj_copy(t->sbsh));
      return ret;
    }
    case _obj::subarg_arithmetic :
    {
      subarg_arithmetic_t* t = dynamic_cast<subarg_arithmetic_t*>(o);
      subarg_arithmetic_t* ret = new subarg_arithmetic_t(*t);
      ret->arith = dynamic_cast<arithmetic_t*>(obj_copy(t->arith));
      return ret;
    }
    case _obj::arithmetic_number :
    {
      arithmetic_number_t* t = dynamic_cast<arithmetic_number_t*>(o);
      arithmetic_number_t* ret = new arithmetic_number_t(*t);
      return ret;
    }
    case _obj::arithmetic_variable :
    {
      arithmetic_variable_t* t = dynamic_cast<arithmetic_variable_t*>(o);
      arithmetic_variable_t* ret = new arithmetic_variable_t(*t);
      ret->var = dynamic_cast<variable_t*>(obj_copy(t->var));
      return ret;
    }
    case _obj::arithmetic_subshell :
    {
      arithmetic_subshell_t* t = dynamic_cast<arithmetic_subshell_t*>(o);
      arithmetic_subshell_t* ret = new arithmetic_subshell_t(*t);
      ret->sbsh = dynamic_cast<subshell_t*>(obj_copy(t->sbsh));
      return ret;
    }
    case _obj::arithmetic_operation :
    {
      arithmetic_operation_t* t = dynamic_cast<arithmetic_operation_t*>(o);
      arithmetic_operation_t* ret = new arithmetic_operation_t(*t);
      ret->val1 = dynamic_cast<arithmetic_t*>(obj_copy(t->val1));
      ret->val2 = dynamic_cast<arithmetic_t*>(obj_copy(t->val2));
      return ret;
    }
    case _obj::arithmetic_parenthesis :
    {
      arithmetic_parenthesis_t* t = dynamic_cast<arithmetic_parenthesis_t*>(o);
      arithmetic_parenthesis_t* ret = new arithmetic_parenthesis_t(*t);
      ret->val = dynamic_cast<arithmetic_t*>(obj_copy(t->val));
      return ret;
    }

    default: return nullptr; //dummy
  }
}


#endif //RECURSIVE_HPP
