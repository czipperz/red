#include "token.hpp"

#include <cz/write.hpp>

namespace cz {

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
        case Token::Set:
            return write(writer, '=');
        case Token::Equals:
            return write(writer, "==");
        case Token::NotEquals:
            return write(writer, "!=");
        case Token::Dot:
            return write(writer, '.');
        case Token::Comma:
            return write(writer, ',');
        case Token::Plus:
            return write(writer, '+');
        case Token::Minus:
            return write(writer, '-');
        case Token::Divide:
            return write(writer, '/');
        case Token::Star:
            return write(writer, '*');
        case Token::Ampersand:
            return write(writer, '&');
        case Token::And:
            return write(writer, "&&");
        case Token::Pipe:
            return write(writer, '|');
        case Token::Or:
            return write(writer, "||");
        case Token::Semicolon:
            return write(writer, ';');
        case Token::Not:
            return write(writer, '!');
        case Token::QuestionMark:
            return write(writer, '?');
        case Token::Colon:
            return write(writer, ':');
        case Token::ColonColon:
            return write(writer, "::");
        case Token::Hash:
            return write(writer, '#');
        case Token::HashHash:
            return write(writer, "##");
        case Token::String: {
            CZ_TRY(write(writer, '"'));
            for (size_t i = 0; i < token.v.string.len; ++i) {
                char c = token.v.string[i];
                if (c == '\\' || c == '"') {
                    CZ_TRY(write(writer, '\\'));
                } else if (c == '\n') {
                    CZ_TRY(write(writer, '\\'));
                    c = 'n';
                } else if (c == '\t') {
                    CZ_TRY(write(writer, '\\'));
                    c = 't';
                } else if (c == '\f') {
                    CZ_TRY(write(writer, '\\'));
                    c = 'f';
                } else if (c == '\r') {
                    CZ_TRY(write(writer, '\\'));
                    c = 'r';
                } else if (c == '\v') {
                    CZ_TRY(write(writer, '\\'));
                    c = 'v';
                }
                CZ_TRY(write(writer, c));
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
    }

    CZ_PANIC("Unimplemented stringify Token variant");
}

}
