  #ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>
#include <utility>
#include <vector>
#include <tuple>

#define SPACES          " \t"
#define SEPARATORS      " \t\n"
#define ARG_END         " \t\n;#()&|<>"
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
#define ARRAY_ARG_END " \t\n;#()&|<>]"

// structs

struct list_parse_options {
  char end_char=0;
  bool word_mode=false;
  std::vector<std::string> end_words={};
  const char* expecting=NULL;
};

// globals

extern const std::vector<std::string> posix_cmdvar;
extern const std::vector<std::string> bash_cmdvar;

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

// list
// std::pair<list*, parse_context> parse_list_until(parse_context ct, char end_c, const char* expecting=NULL);
// std::pair<list*, parse_context> parse_list_until(parse_context ct, std::string const& end_word);
// std::tuple<list*, parse_context, std::string> parse_list_until(parse_context ct, std::vector<std::string> const& end_words, const char* expecting=NULL);
std::tuple<list*, parse_context, std::string> parse_list_until(parse_context ct, list_parse_options opts={});
// name
std::pair<variable*, parse_context> parse_var(parse_context ct, bool specialvars=true, bool array=false);

// subarg parsers
std::pair<arithmetic*, parse_context> parse_arithmetic(parse_context ct);
std::pair<variable*, parse_context> parse_manipulation(parse_context ct);
// arg parser
std::pair<arg*, parse_context> parse_arg(parse_context ct, const char* end=ARG_END, const char* unexpected=ARGLIST_END, bool doquote=true);
// redirect parser
std::pair<redirect*, parse_context> parse_redirect(parse_context ct);
// arglist parser
std::pair<arglist*, parse_context> parse_arglist(parse_context ct, bool hard_error=false, std::vector<redirect*>* redirs=nullptr);
// block parsers
std::pair<block*, parse_context> parse_block(parse_context ct);
std::pair<cmd*, parse_context> parse_cmd(parse_context ct);
std::pair<function*, parse_context> parse_function(parse_context ct, const char* after="()");
std::pair<subshell*, parse_context> parse_subshell(parse_context ct);
std::pair<brace*, parse_context> parse_brace(parse_context ct);
std::pair<case_block*, parse_context> parse_case(parse_context ct);
std::pair<if_block*, parse_context> parse_if(parse_context ct);
std::pair<for_block*, parse_context> parse_for(parse_context ct);
std::pair<while_block*, parse_context> parse_while(parse_context ct);
// pipeline parser
std::pair<pipeline*, parse_context> parse_pipeline(parse_context ct);
// condlist parser
std::pair<condlist*, parse_context> parse_condlist(parse_context ct);

#endif //PARSE_HPP
