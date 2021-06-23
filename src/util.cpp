#include "util.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <tuple>

#include <iostream>
#include <fstream>

#include <ztd/shell.hpp>
#include <ztd/color.hpp>

std::string indenting_string="\t";

std::string indent(int n)
{
  std::string ret;
  for(int i=0; i<n; i++)
    ret += indenting_string;
  return ret;
}

std::string cut_last(std::string const& in, char c)
{
  size_t slr=in.rfind(c);
  if(slr != std::string::npos)
    return in.substr(slr+1);
  else
    return in;
}

std::string basename(std::string const& in)
{
  return cut_last(cut_last(in, '/'), ' ');
}

std::string dirname(std::string const& in)
{
  size_t slr=in.rfind('/');
  if(slr != std::string::npos)
    return in.substr(0,slr);
  else
    return ".";
}

bool is_among(std::string const& in, std::vector<std::string> const& values)
{
  for(auto it: values)
    if(in == it)
      return true;
  return false;
}

std::vector<std::string> split(std::string const& in, const char* splitters)
{
  uint32_t i=0,j=0;
  std::vector<std::string> ret;
  // skip first splitters
  while(i<in.size() && is_in(in[i], splitters))
    i++;

  j=i;
  while(j<in.size())
  {
    while(i<in.size() && !is_in(in[i], splitters)) // count all non-splitters
      i++;
    ret.push_back(in.substr(j,i-j));
    i++;
    while(i<in.size() && is_in(in[i], splitters)) // skip splitters
      i++;
    j=i;
  }
  return ret;
}

std::vector<std::string> split(std::string const& in, char c)
{
  size_t i=0,j=0;
  std::vector<std::string> ret;
  while(j<in.size())
  {
    i=in.find(c, j);
    ret.push_back(in.substr(j,i-j));
    if(i==std::string::npos)
      return ret;
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

std::string concatargs(std::vector<std::string> const& args)
{
  std::string ret;
  for(auto it: args)
    ret += it + ' ';
  ret.pop_back();
  return ret;
}

std::set<std::string> prune_matching(std::set<std::string>& in, std::regex re)
{
  std::set<std::string> ret;
  auto it=in.begin();
  auto prev=in.end();
  while(it!=in.end())
  {
    if( std::regex_match(*it, re) )
    {
      ret.insert(*it);
      in.erase(it);
      if(prev == in.end())
        it = in.begin();
      else
      {
        it = prev;
        it++;
      }
    }
    else
    {
      prev=it;
      it++;
    }
  }
  return ret;
}

int _exec(std::string const& bin, std::vector<std::string> const& args)
{
  std::vector<char*> rargs;
  for(auto it=args.begin(); it!=args.end(); it++)
    rargs.push_back((char*) it->c_str());
  rargs.push_back(NULL);

  pid_t pid;

  // forking
  if((pid = fork()) == -1)
  {
    perror("fork");
    exit(1);
  }
  // child process
  if(pid == 0)
  {
    setpgid(pid, pid); //Needed so negative PIDs can kill children of /bin/sh
    execvp(bin.c_str(), rargs.data());
    exit(1); // exec didn't work
  }

  int stat;
  // wait for end and get return value
  while (waitpid(pid, &stat, 0) == -1)
  {
    if (errno != EINTR)
    {
      stat = -1;
      break;
    }
  }
  return stat;
}

std::string stringReplace(std::string subject, const std::string& search, const std::string& replace)
{
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos)
  {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}

std::string escape_chars(std::string subject, const char* chars)
{
  for(size_t i=0; i<subject.size(); i++)
  {
    if(is_in(subject[i], chars))
    {
      subject.insert(subject.begin()+i, '\\');
      i++;
    }
  }
  return subject;
}

std::string repeatString(std::string const& str, uint32_t n)
{
  std::string ret;
  for(uint32_t i=0; i<n; i++)
  ret += str;
  return ret;
}

void printFormatError(format_error const& e, bool print_line)
{
  const char* in = e.data();

  uint64_t index = e.where();

  uint64_t i=0, j=0; // j: last newline
  uint64_t line=1; //n: line #
  uint64_t in_size=strlen(in);
  if(index >= 0)
  {
    while(i < in_size && i < index)
    {
      if(in[i] == '\n')
      {
        line++;
        j=i+1;
      }
      i++;
    }
    while(i < in_size && in[i]!='\n')
      i++;
  }
  std::cerr << ztd::color::b_white;
  fprintf(stderr, "%s:%lu:%lu: ", e.origin(), line, index-j+1);

  ztd::color level_color;
  const std::string& level = e.level();
  if(level == "error")
    level_color = ztd::color::b_red;
  else if(level == "warning")
    level_color = ztd::color::b_magenta;
  else if(level == "info")
    level_color = ztd::color::b_cyan;

  std::cerr << level_color << e.level() << ztd::color::none;
  fprintf(stderr, ": %s\n", e.what());
  if(print_line)
  {
    std::cerr << std::string(in+j, i-j) << std::endl;
    std::cerr << repeatString(" ", index-j) << '^' << std::endl;
  }
}
