#include "token.hpp"

#include <ctype.h>
#include <cz/write.hpp>

namespace cz {

static Result write_hex_digit(Writer writer, unsigned char ch) {
    if (ch >= 10) {
        return write(writer, ch + 'A' - 10);
    } else {
        return write(writer, ch + '0');
    }
}

static Result write_char(Writer writer, char c) {
    if (c == '\\' || c == '"' || c == '\'') {
        return write(writer, '\\', c);
    } else if (c == '\n') {
        return write(writer, "\\n");
    } else if (c == '\t') {
        return write(writer, "\\t");
    } else if (c == '\f') {
        return write(writer, "\\f");
    } else if (c == '\r') {
        return write(writer, "\\r");
    } else if (c == '\v') {
        return write(writer, "\\v");
    } else if (!isprint(c)) {
        unsigned char first = ((unsigned char)c & 0xF0) >> 4;
        unsigned char second = (unsigned char)c & 0x0F;
        CZ_TRY(write(writer, "\\x"));
        CZ_TRY(write_hex_digit(writer, first));
        return write_hex_digit(writer, second);
    } else {
        return write(writer, c);
    }
}

Result write(Writer writer, red::Token token) {
    using namespace red;
    switch (token.type) {
        case Token::OpenParen:
            return write(writer, '(');
        case Token::CloseParen:
            return write(writer, ')');
        case Token::OpenCurly:
            return write(writer, '{');
        case Token::CloseCurly:
            return write(writer, '}');
        case Token::OpenSquare:
            return write(writer, '[');
        case Token::CloseSquare:
            return write(writer, ']');

        case Token::LessThan:
            return write(writer, '<');
        case Token::LessEqual:
            return write(writer, "<=");
        case Token::GreaterThan:
            return write(writer, '>');
        case Token::GreaterEqual:
            return write(writer, ">=");
        case Token::Equals:
            return write(writer, "==");
        case Token::NotEquals:
            return write(writer, "!=");
        case Token::Dot:
            return write(writer, '.');
        case Token::Comma:
            return write(writer, ',');

        case Token::Set:
            return write(writer, '=');
        case Token::Plus:
            return write(writer, '+');
        case Token::Minus:
            return write(writer, '-');
        case Token::Divide:
            return write(writer, '/');
        case Token::Star:
            return write(writer, '*');
        case Token::Modulus:
            return write(writer, '%');
        case Token::Ampersand:
            return write(writer, '&');
        case Token::And:
            return write(writer, "&&");
        case Token::Pipe:
            return write(writer, '|');
        case Token::Or:
            return write(writer, "||");
        case Token::Xor:
            return write(writer, '^');
        case Token::LeftShift:
            return write(writer, "<<");
        case Token::RightShift:
            return write(writer, ">>");

        case Token::PlusSet:
            return write(writer, "+=");
        case Token::MinusSet:
            return write(writer, "-=");
        case Token::DivideSet:
            return write(writer, "/=");
        case Token::MultiplySet:
            return write(writer, "*=");
        case Token::ModulusSet:
            return write(writer, "%=");
        case Token::BitAndSet:
            return write(writer, "&=");
        case Token::BitOrSet:
            return write(writer, "|=");
        case Token::BitXorSet:
            return write(writer, "^=");
        case Token::LeftShiftSet:
            return write(writer, "<<=");
        case Token::RightShiftSet:
            return write(writer, ">>=");

        case Token::Semicolon:
            return write(writer, ';');
        case Token::Not:
            return write(writer, '!');
        case Token::QuestionMark:
            return write(writer, '?');
        case Token::Tilde:
            return write(writer, '~');
        case Token::Colon:
            return write(writer, ':');
        case Token::ColonColon:
            return write(writer, "::");
        case Token::Hash:
            return write(writer, '#');
        case Token::HashHash:
            return write(writer, "##");

        case Token::Character: {
            CZ_TRY(write(writer, '\''));
            write_char(writer, token.v.ch);
            CZ_TRY(write(writer, '\''));
        }

        case Token::String: {
            CZ_TRY(write(writer, '"'));
            for (size_t i = 0; i < token.v.string.len; ++i) {
                write_char(writer, token.v.string[i]);
            }
            CZ_TRY(write(writer, '"'));
            return Result::ok();
        }
        case Token::Integer:
            CZ_TRY(write(writer, token.v.integer.value));
            if (token.v.integer.suffix & Integer_Suffix::Unsigned) {
                CZ_TRY(write(writer, 'u'));
            }
            if (token.v.integer.suffix & Integer_Suffix::Long) {
                CZ_TRY(write(writer, 'l'));
            }
            if (token.v.integer.suffix & Integer_Suffix::LongLong) {
                CZ_TRY(write(writer, "ll"));
            }
            return Result::ok();
        case Token::Identifier:
            return write(writer, token.v.identifier.str);

        // Keywords
        case Token::Auto:
            return write(writer, "auto");
        case Token::Break:
            return write(writer, "break");
        case Token::Case:
            return write(writer, "case");
        case Token::Char:
            return write(writer, "char");
        case Token::Const:
            return write(writer, "const");
        case Token::Continue:
            return write(writer, "continue");
        case Token::Default:
            return write(writer, "default");
        case Token::Do:
            return write(writer, "do");
        case Token::Double:
            return write(writer, "double");
        case Token::Else:
            return write(writer, "else");
        case Token::Enum:
            return write(writer, "enum");
        case Token::Extern:
            return write(writer, "extern");
        case Token::Float:
            return write(writer, "float");
        case Token::For:
            return write(writer, "for");
        case Token::Goto:
            return write(writer, "goto");
        case Token::If:
            return write(writer, "if");
        case Token::Int:
            return write(writer, "int");
        case Token::Long:
            return write(writer, "long");
        case Token::Register:
            return write(writer, "register");
        case Token::Return:
            return write(writer, "return");
        case Token::Short:
            return write(writer, "short");
        case Token::Signed:
            return write(writer, "signed");
        case Token::Sizeof:
            return write(writer, "sizeof");
        case Token::Static:
            return write(writer, "static");
        case Token::Struct:
            return write(writer, "struct");
        case Token::Switch:
            return write(writer, "switch");
        case Token::Typedef:
            return write(writer, "typedef");
        case Token::Union:
            return write(writer, "union");
        case Token::Unsigned:
            return write(writer, "unsigned");
        case Token::Void:
            return write(writer, "void");
        case Token::Volatile:
            return write(writer, "volatile");
        case Token::While:
            return write(writer, "while");

        case Token::Preprocessor_Parameter:
        case Token::Preprocessor_Varargs_Parameter_Indicator:
        case Token::Parser_Null_Token:
            CZ_PANIC("Cannot print a special token value");
    }

    CZ_PANIC("Unimplemented stringify Token variant");
}

}
