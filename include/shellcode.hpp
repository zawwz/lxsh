#ifndef SHELLCODE_HPP
#define SHELLCODE_HPP

#include <map>
#include <string>

struct lxsh_fct {
  std::string arguments;
  std::string description;
  const char* code;
};

extern const std::map<const std::string, const struct lxsh_fct> lxsh_shellcode_fcts;

#endif //SHELLCODE_HPP
