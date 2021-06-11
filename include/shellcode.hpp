#ifndef SHELLCODE_HPP
#define SHELLCODE_HPP

#include <string>
#include <set>
#include <vector>
#include <map>

#include "struc.hpp"

struct lxsh_fct {
  std::string arguments;
  std::string description;
  const char* code;
  std::vector<std::string> depends_on=std::vector<std::string>();
};

extern const std::map<const std::string, const lxsh_fct> lxsh_extend_fcts;
extern const std::map<const std::string, const lxsh_fct> lxsh_array_fcts;
extern const std::map<const std::string, const lxsh_fct> lxsh_allfcts;

void add_lxsh_fcts(shmain* sh, std::set<std::string> fcts);

#endif //SHELLCODE_HPP
