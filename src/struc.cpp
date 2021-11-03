#include "struc.hpp"

#include "util.hpp"
#include "options.hpp"

#include <unistd.h>

const std::string cmd_t::empty_string="";

condlist_t::condlist_t(block_t* bl)
{
  type=_obj::condlist;
  parallel=false;
  this->add(new pipeline_t(bl));
}
