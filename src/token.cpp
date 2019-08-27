#include "token.hpp"

#include <ctype.h>
#include <cz/assert.hpp>
#include "text_replacement.hpp"

namespace red {

bool next_token(const FileBuffer& file_buffer,
                size_t* index,
                Token* token_out,
                bool* at_bol,
                cz::mem::Allocated<cz::String>* label_value) {
    size_t point = *index;
top:
    const size_t start = point;
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
            *index = point;
            char next = next_character(file_buffer, &point);
            if (next == '=') {
                token_out->type = Token::LessEqual;
            } else if (next == ':') {
                token_out->type = Token::OpenSquare;
            } else if (next == '%') {
                token_out->type = Token::OpenCurly;
            } else {
                token_out->type = Token::LessThan;
                point = *index;
            }
            break;
        }
        case '>': {
            *index = point;
            if (next_character(file_buffer, &point) == '=') {
                token_out->type = Token::GreaterEqual;
            } else {
                token_out->type = Token::GreaterThan;
                point = *index;
            }
            break;
        }
        case ':': {
            *index = point;
            char next = next_character(file_buffer, &point);
            if (next == '>') {
                token_out->type = Token::CloseSquare;
            } else if (next == ':') {
                token_out->type = Token::Namespace;
            } else {
                token_out->type = Token::Colon;
                point = *index;
            }
            break;
        }
        case '%': {
            *index = point;
            if (next_character(file_buffer, &point) == '>') {
                token_out->type = Token::CloseCurly;
            } else {
                point = start;
                return false;
            }
            break;
        }
        case '=':
            *index = point;
            if (next_character(file_buffer, &point) == '=') {
                token_out->type = Token::Equals;
            } else {
                token_out->type = Token::Set;
                point = *index;
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
            token_out->type = Token::Divide;
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
            *index = point;
            if (next_character(file_buffer, &point) == '#') {
                token_out->type = Token::HashHash;
            } else {
                token_out->type = Token::Hash;
                point = *index;
            }
            break;
        }
        case '"': {
            label_value->object.clear();

            *index = point;
            c = next_character(file_buffer, &point);
            while (c && c != '"') {
                label_value->object.reserve(label_value->allocator, 1);
                label_value->object.push(c);

                *index = point;
                c = next_character(file_buffer, &point);
            }

            if (!c) {
                CZ_PANIC("Error: Unterminated string");  // @UserError
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
                *index = point;
                goto top;
            }

            if (isalpha(c) || c == '_') {
                label_value->object.clear();

                while (1) {
                    label_value->object.reserve(label_value->allocator, 1);
                    label_value->object.push(c);

                    *index = point;
                    c = next_character(file_buffer, &point);
                    if (!isalnum(c) && c != '_') {
                        point = *index;
                        break;
                    }
                }

                token_out->type = Token::Label;
                break;
            }

            if (isdigit(c)) {
                label_value->object.clear();

                while (1) {
                    label_value->object.reserve(label_value->allocator, 1);
                    label_value->object.push(c);

                    *index = point;
                    c = next_character(file_buffer, &point);
                    if (!isdigit(c)) {
                        point = *index;
                        break;
                    }
                }

                token_out->type = Token::Integer;
                break;
            }

            return false;
    }

    token_out->start = start;
    token_out->end = point;
    *index = point;
    return true;
}

}
