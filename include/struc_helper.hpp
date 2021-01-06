#ifndef STRUC_HELPER_HPP
#define STRUC_HELPER_HPP

#include "struc.hpp"

cmd* make_cmd(std::vector<std::string> const& args);
cmd* make_cmd(std::string const& in);
condlist* make_condlist(std::string const& in);

inline bool operator==(arg a, std::string const& b) { return a.equals(b); }

#endif //STRUC_HELPER_HPP
