#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>
#include <vector>
#include <memory>
#include <exception>
#include <stdexcept>

#define INDENT indent(ind)

extern std::string indenting_string;

std::string indent(int n);

std::vector<std::string> split(std::string const& in, char c);

std::string escape_str(std::string const& in);

template<typename ... Args>
std::string strf( const std::string& format, Args ... args )
{
    size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    std::unique_ptr<char[]> buf( new char[ size ] );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

std::string delete_brackets(std::string const& in);

#endif //UTIL_HPP
