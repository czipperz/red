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

constexpr bool token_has_value(Token::Type type) {
    return type == Token::Label || type == Token::String || type == Token::Integer;
}

}
