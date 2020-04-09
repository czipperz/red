#include "lex.hpp"

#include <ctype.h>
#include "context.hpp"
#include "file_contents.hpp"
#include "location.hpp"
#include "token.hpp"

namespace red {
namespace lex {

bool next_character(const File_Contents& file_contents, Location* location, char* out) {
top:
    if (location->index == file_contents.len) {
        return false;
    }

    *out = file_contents.get(location->index);

    if (location->index + 1 < file_contents.len) {
        char next = file_contents.get(location->index + 1);
        if (*out == '?' && location->index + 2 < file_contents.len) {
            if (next == '?') {
                switch (file_contents.get(location->index + 2)) {
                    case '=':
                        *out = '#';
                        break;
                    case '/':
                        *out = '\\';
                        break;
                    case '\'':
                        *out = '^';
                        break;
                    case '(':
                        *out = '[';
                        break;
                    case ')':
                        *out = ']';
                        break;
                    case '!':
                        *out = '|';
                        break;
                    case '<':
                        *out = '{';
                        break;
                    case '>':
                        *out = '}';
                        break;
                    case '-':
                        *out = '~';
                        break;
                    default:
                        ++location->index;
                        ++location->column;
                        *out = '?';
                        return true;
                }
                location->index += 2;
                location->column += 2;

                if (location->index + 1 == file_contents.len) {
                    ++location->index;
                    ++location->column;
                    return true;
                }

                next = file_contents.get(location->index + 1);
            }
        }

        if (*out == '\\' && next == '\n') {
            location->index += 2;
            ++location->line;
            location->column = 0;
            goto top;
        }
    }

    ++location->index;
    if (*out == '\n') {
        ++location->line;
        location->column = 0;
    } else {
        ++location->column;
    }
    return true;
}

bool next_token(Context* context,
                Lexer* lexer,
                const File_Contents& file_contents,
                Location* location,
                Token* token_out,
                bool* at_bol) {
    Location point = *location;
top:
    token_out->span.start = point;
    char c;
    if (!next_character(file_contents, &point, &c)) {
        return false;
    }
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
            char next;
            if (next_character(file_contents, &point, &next)) {
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
            } else {
                token_out->type = Token::LessThan;
                point = *location;
            }
            break;
        }
        case '>': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::GreaterEqual;
            } else {
                token_out->type = Token::GreaterThan;
                point = *location;
            }
            break;
        }
        case ':': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next)) {
                if (next == '>') {
                    token_out->type = Token::CloseSquare;
                } else if (next == ':') {
                    token_out->type = Token::ColonColon;
                } else {
                    token_out->type = Token::Colon;
                    point = *location;
                }
            } else {
                token_out->type = Token::Colon;
                point = *location;
            }
            break;
        }
        case '%': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '>') {
                token_out->type = Token::CloseCurly;
            } else {
                token_out->type = Token::Ampersand;
                point = *location;
            }
            break;
        }
        case '=': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::Equals;
            } else {
                token_out->type = Token::Set;
                point = *location;
            }
            break;
        }
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
        case '/': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next)) {
                if (next == '*') {
                    char prev = 0;
                    while (1) {
                        char next;
                        if (!next_character(file_contents, &point, &next)) {
                            context->report_error({*location, point}, "Unterminated block comment");
                            *location = point;
                            return false;
                        }

                        if (prev == '*' && next == '/') {
                            break;
                        }
                        prev = next;
                    }

                    *location = point;
                    goto top;
                } else {
                    token_out->type = Token::Divide;
                    point = *location;
                }
            } else {
                token_out->type = Token::Divide;
                point = *location;
            }
            break;
        }
        case '*':
            token_out->type = Token::Star;
            break;
        case '&': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '&') {
                token_out->type = Token::And;
            } else {
                token_out->type = Token::Ampersand;
            }
            break;
        }
        case '|': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '|') {
                token_out->type = Token::Or;
            } else {
                token_out->type = Token::Pipe;
                point = *location;
            }
            break;
        }
        case ';':
            token_out->type = Token::Semicolon;
            break;
        case '!':
            token_out->type = Token::Not;
            break;
        case '#': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '#') {
                token_out->type = Token::HashHash;
            } else {
                token_out->type = Token::Hash;
                point = *location;
            }
            break;
        }

        case '"': {
            cz::String value = {};
            Location start = point;

            *location = point;
            while (1) {
                if (!next_character(file_contents, &point, &c)) {
                    context->report_error({start, point}, "Unterminated string");
                    value.drop(lexer->string_buffer_array.allocator());
                    return false;
                }

                if (c == '"') {
                    break;
                }

                value.reserve(lexer->string_buffer_array.allocator(), 1);
                value.push(c);

                *location = point;
            }

            value.realloc(lexer->string_buffer_array.allocator());
            token_out->v.string = value;
            token_out->type = Token::String;
            break;
        }

        case '\n':
            *at_bol = true;
        case ' ':
        case '\v':
        case '\f':
        case '\r':
        case '\t':
            *location = point;
            goto top;

        default:
            if (isalpha(c) || c == '_') {
                cz::String value = {};

                while (1) {
                    value.reserve(lexer->identifier_buffer_array.allocator(), 1);
                    value.push(c);

                    *location = point;
                    if (!next_character(file_contents, &point, &c) || !(isalnum(c) || c == '_')) {
                        point = *location;
                        break;
                    }
                }

                struct Keyword {
                    cz::Str value;
                    Token::Type type;
                };
                Keyword keywords[] = {
                    {"__VAR_ARGS__", Token::Preprocessor_Varargs_Keyword},
                    {"auto", Token::Auto},
                    {"break", Token::Break},
                    {"case", Token::Case},
                    {"char", Token::Char},
                    {"const", Token::Const},
                    {"continue", Token::Continue},
                    {"default", Token::Default},
                    {"do", Token::Do},
                    {"double", Token::Double},
                    {"else", Token::Else},
                    {"enum", Token::Enum},
                    {"extern", Token::Extern},
                    {"float", Token::Float},
                    {"for", Token::For},
                    {"goto", Token::Goto},
                    {"if", Token::If},
                    {"int", Token::Int},
                    {"long", Token::Long},
                    {"register", Token::Register},
                    {"return", Token::Return},
                    {"short", Token::Short},
                    {"signed", Token::Signed},
                    {"sizeof", Token::Sizeof},
                    {"static", Token::Static},
                    {"struct", Token::Struct},
                    {"switch", Token::Switch},
                    {"typedef", Token::Typedef},
                    {"union", Token::Union},
                    {"unsigned", Token::Unsigned},
                    {"void", Token::Void},
                    {"volatile", Token::Volatile},
                    {"while", Token::While},
                };

                for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); ++i) {
                    if (value == keywords[i].value) {
                        token_out->type = keywords[i].type;
                        goto end_parse_label;
                    }
                }

                token_out->type = Token::Identifier;
                token_out->v.identifier = Hashed_Str::from_str(value);

            end_parse_label:
                break;
            }

            if (isdigit(c)) {
                uint64_t value = 0;
                while (1) {
                    *location = point;
                    if (!next_character(file_contents, &point, &c) || !isdigit(c)) {
                        point = *location;
                    }
                    value *= 10;
                    value += c - '0';
                }

                token_out->v.integer = value;
                token_out->type = Token::Integer;
                break;
            }

            return false;
    }

    token_out->span.end = point;
    *location = point;
    return true;
}

}
}
