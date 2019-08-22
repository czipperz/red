#include "token.hpp"

#include <ctype.h>
#include "text_replacement.hpp"

namespace red {

bool next_token(const FileBuffer& file_buffer, size_t* index, Token* token_out) {
top:
    size_t start = *index;
    char c = next_character(file_buffer, index);
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
            size_t index_clone = *index;
            char next = next_character(file_buffer, &index_clone);
            if (next == '=') {
                token_out->type = Token::LessEqual;
                *index = index_clone;
            } else if (next == ':') {
                token_out->type = Token::OpenSquare;
                *index = index_clone;
            } else if (next == '%') {
                token_out->type = Token::OpenCurly;
                *index = index_clone;
            } else {
                token_out->type = Token::LessThan;
            }
            break;
        }
        case '>': {
            size_t index_clone = *index;
            if (next_character(file_buffer, &index_clone) == '=') {
                token_out->type = Token::GreaterEqual;
                *index = index_clone;
            } else {
                token_out->type = Token::GreaterThan;
            }
            break;
        }
        case ':': {
            size_t index_clone = *index;
            if (next_character(file_buffer, &index_clone) == '>') {
                token_out->type = Token::CloseSquare;
                *index = index_clone;
            } else {
                return false;
            }
            break;
        }
        case '%': {
            size_t index_clone = *index;
            if (next_character(file_buffer, &index_clone) == '>') {
                token_out->type = Token::CloseCurly;
                *index = index_clone;
            } else {
                return false;
            }
            break;
        }
        default:
            if (isspace(c)) {
                goto top;
            }
            if (isalpha(c) || c == '_') {
                while (1) {
                    size_t index_clone = *index;
                    c = next_character(file_buffer, &index_clone);
                    if (isalnum(c) || c == '_') {
                        *index = index_clone;
                    } else {
                        break;
                    }
                }

                token_out->type = Token::Label;
                token_out->start = start;
                token_out->end = *index;
                return true;
            }
            return false;
    }
    token_out->start = start;
    token_out->end = *index;
    return true;
}

}
