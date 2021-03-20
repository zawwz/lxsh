#include "shellcode.hpp"

#include "g_shellcode.h"

const std::map<const std::string, const struct lxsh_fct> lxsh_shellcode_fcts = {
  { "_lxsh_random_string",  { "N", "Generate a random alphanumeric string of length N. Default 20", RANDOM_STRING_SH} },
  { "_lxsh_random_tmpfile", { "N", "Get a random TMP filepath, with N random chars. Default 20", RANDOM_TMPFILE_SH} }
};
