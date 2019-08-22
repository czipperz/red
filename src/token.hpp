#pragma once

#include <stddef.h>
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

bool next_token(const FileBuffer& file_buffer, size_t* index, Token* token_out);

}
