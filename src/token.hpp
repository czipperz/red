#pragma once

#include <stdint.h>
#include "hashed_str.hpp"
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
        Colon,
        ColonColon,
        Hash,
        HashHash,
        String,
        Integer,
        Identifier,

        // Keywords
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

        // Special parameter value used in preprocessor definitions
        Preprocessor_Parameter,
        Preprocessor_Varargs_Parameter_Indicator,
        Preprocessor_Varargs_Keyword,
    };

    Type type;
    Span span;
    union Value {
        Hashed_Str identifier;
        cz::Str string;
        uint64_t integer;
    } v;
};

}
