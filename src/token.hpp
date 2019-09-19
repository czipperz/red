#pragma once

#include <stddef.h>
#include <cz/string.hpp>
#include "context.hpp"
#include "file_buffer.hpp"
#include "location.hpp"
#include "span.hpp"

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
        Set,
        Equals,
        Dot,
        Comma,
        Plus,
        Minus,
        Divide,
        Star,
        Semicolon,
        Not,
        Namespace,
        Colon,
        Hash,
        HashHash,
        String,
        Label,
        Integer,
    };

    Type type;
    Span span;
};

namespace cpp {

/**
 * at_bol is an out variable but is only set to true.  Set it to true before
 * calling if at the bof otherwise false.  This dictates whether (when not in a
 * macro) Token::Hash starts a macro.
 */
bool next_token(C* c,
                const FileBuffer& file_buffer,
                Location* location,
                Token* token_out,
                bool* at_bol,
                cz::AllocatedString* label_value);

constexpr bool token_has_value(Token::Type type) {
    return type == Token::Label || type == Token::String || type == Token::Integer;
}

}
}
