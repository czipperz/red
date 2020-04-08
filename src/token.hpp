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
        Ampersand,
        And,
        Pipe,
        Or,
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
    union Value {
        cz::Str label;
        cz::Str string;
        uint64_t integer;
    } v;
};

}
