#include "struc.hpp"

#include "util.hpp"
#include "options.hpp"

#include <unistd.h>

std::string g_origin="";

const std::string cmd::empty_string="";

condlist::condlist(block* bl)
{
  type=_obj::_condlist;
  parallel=false;
  this->add(new pipeline(bl));
}
