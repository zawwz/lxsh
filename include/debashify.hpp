#ifndef DEBASHIFY_HPP
#define DEBASHIFY_HPP

#include "struc.hpp"

#include <map>
#include <set>

struct debashify_params {
  std::set<std::string> required_fcts;
  void require_fct(std::string const& in) { required_fcts.insert(in); }
  // map of detected arrays
  // bool value: is associative
  std::map<std::string,bool> arrays;
};

bool r_debashify(_obj* o, debashify_params* params);

std::set<std::string> debashify(_obj* o, debashify_params* params);
std::set<std::string> debashify(shmain* sh);

#endif //DEBASHIFY_HPP
