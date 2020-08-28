#include "util.hpp"

std::string indenting_string="\t";

std::string indent(int n)
{
  std::string ret;
  for(int i=0; i<n; i++)
    ret += indenting_string;
  return ret;
}

std::vector<std::string> split(std::string const& in, char c)
{
  uint32_t i=0,j=0;
  std::vector<std::string> ret;
  while(j<in.size())
  {
    i=in.find(c, j);
    ret.push_back(in.substr(j,i-j));
    j=i+1;
  }
  return ret;
}

std::string escape_str(std::string const& in)
{
  std::string ret;
  for(uint64_t i=0; i<in.size(); i++)
  {
    if(in[i] == '\n')
      ret += "\\n";
    else if(in[i] == '\"')
      ret += "\\\"";
    else if(in[i] == '\t')
      ret += "\\t";
    else if(in[i] == '\\')
      ret += "\\\\";
    else
      ret += in[i];
  }
  return ret;
}

std::string delete_brackets(std::string const& in)
{
  std::string ret;
  for(auto it: in)
  {
    if(it!='[' && it!=']')
      ret += it;
  }
  return ret;
}
