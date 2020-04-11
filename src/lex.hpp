#pragma once

#include <cz/buffer_array.hpp>
#include "token.hpp"

namespace red {
struct Context;
struct File_Contents;
struct Location;

namespace lex {

struct Lexer {
    cz::Buffer_Array string_buffer_array;
    cz::Buffer_Array identifier_buffer_array;

    void init() {
        string_buffer_array.create();
        identifier_buffer_array.create();
    }

    void drop() {
        string_buffer_array.drop();
        identifier_buffer_array.drop();
    }
};

bool next_character(const File_Contents& file_contents, Location* location, char* out);

/// Get the next token without running the preprocessor.
///
/// `at_bol` is an out variable but is only set to true.  Set it to `true` before calling if at the
/// beginning of the file otherwise `false`.  This dictates whether (when not in a macro)
/// `Token::Hash` starts a macro.
bool next_token(Context* context,
                Lexer* lexer,
                const File_Contents& file_contents,
                Location* location,
                Token* token_out,
                bool* at_bol);

}
}
