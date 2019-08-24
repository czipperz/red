#pragma once

#include <stddef.h>
#include <cz/string.hpp>
#include "file_buffer.hpp"

namespace red {

struct Token {
    enum Type {
        OpenParen,
        CloseParen,
        OpenCurly,
        CloseCurly,
        OpenSquare,
        CloseSquare,
        LessThan,
        LessEqual,
        GreaterThan,
        GreaterEqual,
        Hash,
        HashHash,
        Label,
    } type;
    size_t start, end;
};

/**
 * at_bol is an out variable but is only set to true.  Set it to true before
 * calling if at the bof otherwise false.  This dictates whether (when not in a
 * macro) Token::Hash starts a macro.
 */
bool next_token(const FileBuffer& file_buffer,
                size_t* index,
                Token* token_out,
                bool* at_bol,
                cz::mem::Allocated<cz::String>* label_value);

}
