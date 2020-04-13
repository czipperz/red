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
        NotEquals,
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
        QuestionMark,
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

        // Special case used for peek.
        Parser_Null_Token,
    };

    Type type;
    Span span;
    union Value {
        Hashed_Str identifier;
        cz::Str string;
        struct {
            uint64_t value;
            uint32_t suffix;
        } integer;
    } v;
};

namespace Integer_Suffix_ {
enum Integer_Suffix : uint32_t {
    Unsigned,
    Long,
    LongLong,
};
}
using Integer_Suffix_::Integer_Suffix;

}
