#ifndef DEBASHIFY_HPP
#define DEBASHIFY_HPP

#include "struc.hpp"

#include <map>

typedef struct debashify_params {
  bool need_random_string=false;
  bool need_random_tmpfile=false;
  bool need_array_create=false;
  bool need_array_set=false;
  bool need_array_get=false;
  bool need_map_create=false;
  bool need_map_set=false;
  bool need_map_get=false;
  // map of detected arrays
  // bool value: is associative
  std::map<std::string,bool> arrays;
} debashify_params;

bool r_debashify(_obj* o, debashify_params* params);

void debashify(_obj* o, debashify_params* params);
void debashify(shmain* sh);

#endif //DEBASHIFY_HPP
