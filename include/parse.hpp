  #ifndef PARSE_HPP
#define PARSE_HPP

#include "struc.hpp"

#include <string>
#include <utility>
#include <vector>

#include <ztd/filedat.hpp>

#define SPACES          " \t"
#define SEPARATORS      " \t\n"
#define ARG_END         " \t\n;#()&|<>"
#define VARNAME_END     " \t\n;#()&|=\"'\\{}/-+"
#define BLOCK_TOKEN_END " \t\n;#()&|=\"'\\"
#define COMMAND_SEPARATOR  "\n;"
#define CONTROL_END           "#)"
#define PIPELINE_END       "\n;#()&"
#define ARGLIST_END        "\n;#()&|"
#define SPECIAL_TOKENS     "\n;#()&|"
#define ALL_TOKENS         "\n;#()&|{}"

#define SPECIAL_VARS "!#*@$?"

// bash specific
#define ARRAY_ARG_END " \t\n;#()&|<>]"

extern const std::vector<std::string> posix_cmdvar;
extern const std::vector<std::string> bash_cmdvar;

std::string import_file(std::string const& path);

shmain* parse_text(const char* in, uint32_t size, std::string const& filename="");
inline shmain* parse_text(std::string const& in, std::string const& filename="") { return parse_text(in.c_str(), in.size(), filename); }
inline shmain* parse(std::string const& file) { return parse_text(import_file(file), file); }

// ** unit parsers ** //

/* util parsers */
// list
std::pair<list*, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, char end_c, const char* expecting=NULL);
std::pair<list*, uint32_t> parse_list_until(const char* in, uint32_t size, uint32_t start, std::string const& end_word);
std::tuple<list*, uint32_t, std::string> parse_list_until(const char* in, uint32_t size, uint32_t start, std::vector<std::string> const& end_words, const char* expecting=NULL);
// name
std::pair<variable*, uint32_t> parse_var(const char* in, uint32_t size, uint32_t start, bool specialvars=true, bool array=false);

// subarg parsers
std::pair<arithmetic*, uint32_t> parse_arithmetic(const char* in, uint32_t size, uint32_t start);
std::pair<variable*, uint32_t> parse_manipulation(const char* in, uint32_t size, uint32_t start);
// arg parser
std::pair<arg*, uint32_t> parse_arg(const char* in, uint32_t size, uint32_t start, const char* end=ARG_END, const char* unexpected=SPECIAL_TOKENS, bool doquote=true);
// redirect parser
std::pair<redirect*, uint32_t> parse_redirect(const char* in, uint32_t size, uint32_t start);
// arglist parser
std::pair<arglist*, uint32_t> parse_arglist(const char* in, uint32_t size, uint32_t start, bool hard_error=false, std::vector<redirect*>* redirs=nullptr);
// block parsers
std::pair<block*, uint32_t> parse_block(const char* in, uint32_t size, uint32_t start);
std::pair<cmd*, uint32_t> parse_cmd(const char* in, uint32_t size, uint32_t start);
std::pair<function*, uint32_t> parse_function(const char* in, uint32_t size, uint32_t start, const char* after="()");
std::pair<subshell*, uint32_t> parse_subshell(const char* in, uint32_t size, uint32_t start);
std::pair<brace*, uint32_t> parse_brace(const char* in, uint32_t size, uint32_t start);
std::pair<case_block*, uint32_t> parse_case(const char* in, uint32_t size, uint32_t start);
std::pair<if_block*, uint32_t> parse_if(const char* in, uint32_t size, uint32_t start);
std::pair<for_block*, uint32_t> parse_for(const char* in, uint32_t size, uint32_t start);
std::pair<while_block*, uint32_t> parse_while(const char* in, uint32_t size, uint32_t start);
// pipeline parser
std::pair<pipeline*, uint32_t> parse_pipeline(const char* in, uint32_t size, uint32_t start);
// condlist parser
std::pair<condlist*, uint32_t> parse_condlist(const char* in, uint32_t size, uint32_t start);

#endif //PARSE_HPP
