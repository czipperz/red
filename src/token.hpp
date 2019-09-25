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
        Integer,
        Label,
        // keywords
        Auto,
        Break,
        Case,
        Char,
        Const,
        Continue,
        Default,
        Do,
        Double,
        Else,
        Enum,
        Extern,
        Float,
        For,
        Goto,
        If,
        Int,
        Long,
        Register,
        Return,
        Short,
        Signed,
        Sizeof,
        Static,
        Struct,
        Switch,
        Typedef,
        Union,
        Unsigned,
        Void,
        Volatile,
        While,
    };

    Type type;
    Span span;
};

constexpr bool token_has_value(Token::Type type) {
    return type == Token::Label || type == Token::String || type == Token::Integer;
}

}
