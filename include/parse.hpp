  #ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>
#include <utility>
#include <vector>
#include <set>
#include <tuple>

#define SPACES          " \t"
#define SEPARATORS      " \t\n"
#define ARG_END         " \t\n;()&|<>"
#define VARNAME_END     " \t\n;#()&|=\"'\\{}/-+"
#define BLOCK_TOKEN_END " \t\n;#()&|=\"'\\"
#define BASH_BLOCK_END  " \t\n;#()&|=\"'\\{}"
#define COMMAND_SEPARATOR  "\n;"
#define CONTROL_END           "#)"
#define PIPELINE_END       "\n;#()&"
#define ARGLIST_END        "\n;#()&|"
#define ALL_TOKENS         "\n;#()&|{}"

#define ARITHMETIC_OPERATOR_END " \t\n$)"

#define SPECIAL_VARS "!#*@$?"

// bash specific
#define ARRAY_ARG_END       " \t\n;#()&|<>]"
// optimizations
#define ARG_OPTIMIZE_NULL     "$\\`"
#define ARG_OPTIMIZE_MANIP    "$\\`}"
#define ARG_OPTIMIZE_DEFARR   "$\\`)"
#define ARG_OPTIMIZE_BASHTEST "$\\`] \t\n"
#define ARG_OPTIMIZE_ARG      "$\\` \t\n;()&|<>\"'"
#define ARG_OPTIMIZE_ARRAY    "$\\`\t\n&|}[]\"'"
#define ARG_OPTIMIZE_ALL      "$\\` \t\n;#()&|<>}]\"'"

// structs

struct list_parse_options {
  char end_char=0;
  bool word_mode=false;
  std::vector<std::string> end_words={};
  const char* expecting=NULL;
};

// globals
extern const std::set<std::string> all_reserved_words;
extern const std::set<std::string> posix_cmdvar;
extern const std::set<std::string> bash_cmdvar;

std::string import_file(std::string const& path);

std::pair<shmain*, parse_context> parse_text(parse_context context);
std::pair<shmain*, parse_context> parse_text(std::string const& in, std::string const& filename="");
inline std::pair<shmain*, parse_context> parse(std::string const& file) { return parse_text(import_file(file), file); }

// tools

parse_context make_context(std::string const& in, std::string const& filename="", bool bash=false);
parse_context make_context(parse_context ctx, std::string const& in="", std::string const& filename="", bool bash=false);
parse_context make_context(parse_context ctx, uint64_t i);
parse_context operator+(parse_context ctx, int64_t a);
parse_context operator-(parse_context ctx, int64_t a);

// error handlers
void parse_error(std::string const& message, parse_context& ctx);
void parse_error(std::string const& message, parse_context& ctx, uint64_t i);

// ** unit parsers ** //

/* util parsers */
uint32_t word_eq(const char* word, const char* in, uint32_t size, uint32_t start, const char* end_set=NULL);
inline bool word_eq(const char* word, parse_context const& ct, const char* end_set=NULL) {
  return word_eq(word, ct.data, ct.size, ct.i, end_set);
}
std::pair<std::string,uint32_t> get_word(parse_context ct, const char* end_set);
uint32_t skip_chars(const char* in, uint32_t size, uint32_t start, const char* set);
inline uint32_t skip_chars(parse_context const& ct, const char* set) {
  return skip_chars(ct.data, ct.size, ct.i, set);
}
uint32_t skip_until(const char* in, uint32_t size, uint32_t start, const char* set);
inline uint32_t skip_until(parse_context const& ct, const char* set) {
  return skip_until(ct.data, ct.size, ct.i, set);
}
uint32_t skip_unread(const char* in, uint32_t size, uint32_t start);
inline uint32_t skip_unread(parse_context const& ct) {
  return skip_unread(ct.data, ct.size, ct.i);
}
uint32_t skip_unread_noline(const char* in, uint32_t size, uint32_t start);
inline uint32_t skip_unread_noline(parse_context const& ct) {
  return skip_unread_noline(ct.data, ct.size, ct.i);
}

// heredocument
parse_context parse_heredocument(parse_context ctx);

// list
std::tuple<list_t*, parse_context, std::string> parse_list_until(parse_context ct, list_parse_options opts={});
// name
std::pair<variable_t*, parse_context> parse_var(parse_context ct, bool specialvars=true, bool array=false);

// subarg parsers
std::pair<arithmetic_t*, parse_context> parse_arithmetic(parse_context ct);
std::pair<variable_t*, parse_context> parse_manipulation(parse_context ct);
// arg parser
std::pair<arg_t*, parse_context> parse_arg(parse_context ct, const char* end=ARG_END, const char* unexpected=ARGLIST_END, bool doquote=true, const char* optimize=ARG_OPTIMIZE_ARG);
// redirect parser
std::pair<redirect_t*, parse_context> parse_redirect(parse_context ct);
// arglist parser
std::pair<arglist_t*, parse_context> parse_arglist(parse_context ct, bool hard_error=false, std::vector<redirect_t*>* redirs=nullptr, bool stop_on_brace=false);
// block parsers
std::pair<block_t*, parse_context> parse_block(parse_context ct);
std::pair<cmd_t*, parse_context> parse_cmd(parse_context ct);
std::pair<function_t*, parse_context> parse_function(parse_context ct, const char* after="()");
std::pair<subshell_t*, parse_context> parse_subshell(parse_context ct);
std::pair<brace_t*, parse_context> parse_brace(parse_context ct);
std::pair<case_t*, parse_context> parse_case(parse_context ct);
std::pair<if_t*, parse_context> parse_if(parse_context ct);
std::pair<for_t*, parse_context> parse_for(parse_context ct);
std::pair<while_t*, parse_context> parse_while(parse_context ct);
// pipeline parser
std::pair<pipeline_t*, parse_context> parse_pipeline(parse_context ct);
// condlist parser
std::pair<condlist_t*, parse_context> parse_condlist(parse_context ct);

#endif //PARSE_HPP
