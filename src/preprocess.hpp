#pragma once

#include <cz/str_map_removable.hpp>
#include <cz/vector.hpp>
#include "location.hpp"

namespace red {
namespace lex {
struct Lexer;
}

struct Context;
struct Result;
struct Span;
struct Token;

namespace pre {
struct Definition;

struct Include_Info {
    Location location;
    cz::Vector<Span> if_stack;
};

struct Definition_Info {
    // Since it is imposible to change the definitions table while in a definition, it is safe to
    // store a pointer here.
    Definition* definition;
    size_t index;
    size_t argument_index;
    cz::Vector<cz::Vector<Token> > arguments;
};

struct Preprocessor {
    cz::Vector<bool> file_pragma_once;
    cz::Str_Map_Removable<Definition> definitions;

    cz::Vector<Include_Info> include_stack;
    cz::Vector<Definition_Info> definition_stack;

    void destroy();

    Location location() const;
};

Result next_token(Context* context, Preprocessor* preprocessor, lex::Lexer* lexer, Token* token);

}

namespace cpp {

Result next_token(Context* context,
                  pre::Preprocessor* preprocessor,
                  lex::Lexer* lexer,
                  Token* token);

}
}
