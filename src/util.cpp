#include "util.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <tuple>

#include <iostream>

#include <ztd/shell.hpp>

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

std::string pwd()
{
  char buf[2048];
  if(getcwd(buf, 2048) != NULL)
  {
    std::string ret=ztd::exec("pwd").first; // getcwd failed: call pwd
    ret.pop_back();
    return ret;
  }
  return std::string(buf);
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

std::string repeatString(std::string const& str, uint32_t n)
{
  std::string ret;
  for(uint32_t i=0; i<n; i++)
  ret += str;
  return ret;
}

void printFormatError(ztd::format_error const& e, bool print_line)
{
  printErrorIndex(e.data(), e.where(), e.what(), e.origin(), print_line);
}

void printErrorIndex(const char* in, const int index, const std::string& message, const std::string& origin, bool print_line)
{
  int i=0, j=0; // j: last newline
  int line=1; //n: line #
  int in_size=strlen(in);
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
    {
      i++;
    }
  }
  if(origin != "")
  {
    fprintf(stderr, "%s:%u:%u: %s\n", origin.c_str(), line, index-j+1, message.c_str());
    if(print_line)
    {
      std::cerr << std::string(in+j, i-j) << std::endl;
      std::cerr << repeatString(" ", index-j) << '^' << std::endl;
    }
  }
}
