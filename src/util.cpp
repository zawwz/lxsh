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

std::string basename(std::string const& in)
{
  size_t slr=in.rfind('/');
  if(slr != std::string::npos)
    return in.substr(slr);
  else
    return in;
}

std::string dirname(std::string const& in)
{
  size_t slr=in.rfind('/');
  if(slr != std::string::npos)
    return in.substr(0,slr);
  else
    return ".";
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

void concat_sets(std::set<std::string>& a, std::set<std::string> const& b)
{
  for(auto it: b)
  {
    a.insert( it );
  }
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


int execute(shmain* sh, std::vector<std::string>& args)
{
  std::string data=sh->generate();

  std::string filename = basename(args[0]);

  // generate path
  std::string tmpdir = (getenv("TMPDIR") != NULL) ? getenv("TMPDIR") : "/tmp" ;
  std::string dirpath = tmpdir + "/lxsh_" + ztd::sh("tr -dc '[:alnum:]' < /dev/urandom | head -c10");
  std::string filepath = dirpath+'/'+filename;

  // create dir
  if(ztd::exec("mkdir", "-p", dirpath).second)
  {
    throw std::runtime_error("Failed to create directory '"+dirpath+'\'');
  }

  // create stream
  std::ofstream stream(filepath);
  if(!stream)
  {
    ztd::exec("rm", "-rf", dirpath);
    throw std::runtime_error("Failed to write to '"+filepath+'\'');
  }

  // output
  stream << data;
  stream.close();
  if(ztd::exec("chmod", "+x", filepath).second != 0)
  {
    ztd::exec("rm", "-rf", dirpath);
    throw std::runtime_error("Failed to make '"+filepath+"' executable");
  }

  // exec
  int retval=_exec(filepath, args);
  ztd::exec("rm", "-rf", dirpath);

  return retval;
}
