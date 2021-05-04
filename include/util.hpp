#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>
#include <vector>
#include <memory>
#include <exception>
#include <stdexcept>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <regex>

#include "struc.hpp"

extern std::string indenting_string;

std::string cut_last(std::string const& in, char c);
std::string basename(std::string const& in);
std::string dirname(std::string const& in);

inline bool is_dev_file(std::string const& filename) { return filename.substr(0,5) == "/dev/"; }

std::string indent(int n);

bool is_among(std::string const& in, std::vector<std::string> const& values);

std::vector<std::string> split(std::string const& in, const char* splitters);
std::vector<std::string> split(std::string const& in, char c);

std::string escape_str(std::string const& in);

inline bool is_num(char c) { return (c >= '0' && c <= '9'); }
inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_alphanum(char c) { return is_alpha(c) || is_num(c); }

template<typename ... Args>
std::string strf( const std::string& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size <= 0 )
      throw std::runtime_error( "Error during formatting." );
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

template<class T, typename ... Args>
std::vector<T> make_vector(Args ... args)
{
  return std::vector<T>( { args... } );
}

template <class KEY, class VAL>
std::vector<std::pair<KEY, VAL>> sort_by_value(std::map<KEY,VAL> const& in)
{
  typedef std::pair<KEY,VAL> pair_t;
  // create an empty vector of pairs
  std::vector<pair_t> ret;

  // copy key-value pairs from the map to the vector
  std::copy(in.begin(),
  in.end(),
  std::back_inserter<std::vector<pair_t>>(ret));

  // sort the vector by increasing order of its pair's second value
  // if second value are equal, order by the pair's first value
  std::sort(ret.begin(), ret.end(),
    [](const pair_t& l, const pair_t& r) {
      if (l.second != r.second)
        return l.second > r.second;
      return l.first > r.first;
    });
  return ret;
}

inline bool is_in(char c, const char* set) {
  return strchr(set, c) != NULL;
}

template <class T>
std::set<std::string> prune_matching(std::map<std::string, T>& in, std::regex re)
{
  std::set<std::string> ret;
  auto it=in.begin();
  auto prev=in.end();
  while(it!=in.end())
  {
    if( std::regex_match(it->first, re) )
    {
      ret.insert(it->first);
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

template <class T, class T2>
std::set<T> map_to_set(std::map<T,T2> in)
{
  std::set<T> ret;
  for(auto it: in)
  {
    ret.insert(it.first);
  }
  return ret;
}

template <class T>
void concat_sets(std::set<T>& a, std::set<T> const& b)
{
  for(auto it: b)
  {
    a.insert( it );
  }
}

template <class T>
bool is_in_vector(T el, std::vector<T> vec)
{
  for(auto it: vec)
    if(it == el)
      return true;
  return false;
}

std::set<std::string> prune_matching(std::set<std::string>& in, std::regex re);

std::string delete_brackets(std::string const& in);

std::string concatargs(std::vector<std::string> const& args);

int _exec(std::string const& bin, std::vector<std::string> const& args);

std::string stringReplace(std::string subject, const std::string& search, const std::string& replace);

void printFormatError(format_error const& e, bool print_line=true);
void printErrorIndex(const char* in, const int index, const std::string& message, const std::string& origin, bool print_line=true);

int execute(shmain* sh, std::vector<std::string>& args);

#endif //UTIL_HPP
