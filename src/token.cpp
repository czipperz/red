#include "token.hpp"

#include <ctype.h>
#include <cz/assert.hpp>
#include "text_replacement.hpp"

namespace red {
namespace cpp {

bool next_token(C* ctxt,
                const FileBuffer& file_buffer,
                Location* location,
                Token* token_out,
                bool* at_bol,
                cz::AllocatedString* label_value) {
    Location point = *location;
top:
    const Location start = point;
    char c = next_character(file_buffer, &point);
    switch (c) {
        case '(':
            token_out->type = Token::OpenParen;
            break;
        case ')':
            token_out->type = Token::CloseParen;
            break;
        case '{':
            token_out->type = Token::OpenCurly;
            break;
        case '}':
            token_out->type = Token::CloseCurly;
            break;
        case '[':
            token_out->type = Token::OpenSquare;
            break;
        case ']':
            token_out->type = Token::CloseSquare;
            break;
        case '<': {
            *location = point;
            char next = next_character(file_buffer, &point);
            if (next == '=') {
                token_out->type = Token::LessEqual;
            } else if (next == ':') {
                token_out->type = Token::OpenSquare;
            } else if (next == '%') {
                token_out->type = Token::OpenCurly;
            } else {
                token_out->type = Token::LessThan;
                point = *location;
            }
            break;
        }
        case '>': {
            *location = point;
            if (next_character(file_buffer, &point) == '=') {
                token_out->type = Token::GreaterEqual;
            } else {
                token_out->type = Token::GreaterThan;
                point = *location;
            }
            break;
        }
        case ':': {
            *location = point;
            char next = next_character(file_buffer, &point);
            if (next == '>') {
                token_out->type = Token::CloseSquare;
            } else if (next == ':') {
                token_out->type = Token::Namespace;
            } else {
                token_out->type = Token::Colon;
                point = *location;
            }
            break;
        }
        case '%': {
            *location = point;
            if (next_character(file_buffer, &point) == '>') {
                token_out->type = Token::CloseCurly;
            } else {
                point = start;
                return false;
            }
            break;
        }
        case '=':
            *location = point;
            if (next_character(file_buffer, &point) == '=') {
                token_out->type = Token::Equals;
            } else {
                token_out->type = Token::Set;
                point = *location;
            }
            break;
        case '.':
            token_out->type = Token::Dot;
            break;
        case ',':
            token_out->type = Token::Comma;
            break;
        case '+':
            token_out->type = Token::Plus;
            break;
        case '-':
            token_out->type = Token::Minus;
            break;
        case '/':
            *location = point;
            c = next_character(file_buffer, &point);
            if (c == '*') {
                char c = next_character(file_buffer, &point);
                while (1) {
                    if (!c) {
                        ctxt->report_error({*location, point}, "Unterminated block comment");
                        *location = point;
                        return false;
                    }

                    char next = next_character(file_buffer, &point);
                    if (c == '*' && next == '/') {
                        break;
                    }
                    c = next;
                }
                *location = point;
                goto top;
            } else {
                token_out->type = Token::Divide;
                point = *location;
            }
            break;
        case '*':
            token_out->type = Token::Star;
            break;
        case ';':
            token_out->type = Token::Semicolon;
            break;
        case '!':
            token_out->type = Token::Not;
            break;
        case '#': {
            *location = point;
            if (next_character(file_buffer, &point) == '#') {
                token_out->type = Token::HashHash;
            } else {
                token_out->type = Token::Hash;
                point = *location;
            }
            break;
        }
        case '"': {
            label_value->clear();

            Location start = point;

            *location = point;
            c = next_character(file_buffer, &point);
            while (c && c != '"') {
                label_value->reserve(1);
                label_value->push(c);

                *location = point;
                c = next_character(file_buffer, &point);
            }

            if (!c) {
                ctxt->report_error({start, point}, "Unterminated string");
                return false;
            }

            token_out->type = Token::String;
            break;
        }
        default:
            if (isspace(c)) {
                if (c == '\n') {
                    *at_bol = true;
                }
                *location = point;
                goto top;
            }

            if (isalpha(c) || c == '_') {
                label_value->clear();

                while (1) {
                    label_value->reserve(1);
                    label_value->push(c);

                    *location = point;
                    c = next_character(file_buffer, &point);
                    if (!isalnum(c) && c != '_') {
                        point = *location;
                        break;
                    }
                }

                token_out->type = Token::Label;
                break;
            }

            if (isdigit(c)) {
                label_value->clear();

                while (1) {
                    label_value->reserve(1);
                    label_value->push(c);

                    *location = point;
                    c = next_character(file_buffer, &point);
                    if (!isdigit(c)) {
                        point = *location;
                        break;
                    }
                }

                token_out->type = Token::Integer;
                break;
            }

            return false;
    }

    token_out->span.start = start;
    token_out->span.end = point;
    *location = point;
    return true;
}

}
}
