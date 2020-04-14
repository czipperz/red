#include "lex.hpp"

#include <ctype.h>
#include <Tracy.hpp>
#include "context.hpp"
#include "file_contents.hpp"
#include "location.hpp"
#include "token.hpp"

namespace red {
namespace lex {

bool next_character(const File_Contents& file_contents, Location* location, char* out) {
    ZoneScoped;
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

static bool process_escaped_string(char* c) {
    switch (*c) {
        case '\\':
        case '"':
            return true;
        case 'n':
            *c = '\n';
            return true;
        case 't':
            *c = '\t';
            return true;
        case 'f':
            *c = '\f';
            return true;
        case 'r':
            *c = '\r';
            return true;
        case 'v':
            *c = '\v';
            return true;
        case '0':
            *c = '\0';
            return true;
        default:
            return false;
    }
}

bool next_token(Context* context,
                Lexer* lexer,
                const File_Contents& file_contents,
                Location* location,
                Token* token_out,
                bool* at_bol) {
    ZoneScopedN("lex::next_token");
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
        case '.': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '.') {
                if (next_character(file_contents, &point, &next) && next == '.') {
                    token_out->type = Token::Preprocessor_Varargs_Parameter_Indicator;
                } else {
                    token_out->type = Token::Dot;
                    point = *location;
                }
            } else {
                token_out->type = Token::Dot;
                point = *location;
            }
            break;
        }
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
                    ZoneScopedN("lex::next_token block comment");
                    char prev = 0;
                    while (1) {
                        char next;
                        if (!next_character(file_contents, &point, &next)) {
                            context->report_lex_error({*location, point},
                                                      "Unterminated block comment");
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
                } else if (next == '/') {
                    ZoneScopedN("lex::next_token line comment");
                    while (1) {
                        char next;
                        if (!next_character(file_contents, &point, &next)) {
                            *location = point;
                            return false;
                        }

                        if (next == '\n') {
                            break;
                        }
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
        case '?':
            token_out->type = Token::QuestionMark;
            break;
        case '~':
            token_out->type = Token::Tilde;
            break;
        case '!': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::NotEquals;
            } else {
                token_out->type = Token::Not;
                point = *location;
            }
            break;
        }
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

        case '\'': {
            ZoneScopedN("lex::next_token character");

            Location start = point;

            *location = point;

            if (!next_character(file_contents, &point, &c)) {
                context->report_lex_error({start, point}, "Unterminated character literal");
                return false;
            }

            if (c == '\\') {
                if (!next_character(file_contents, &point, &c)) {
                    context->report_lex_error({start, point}, "Unterminated character literal");
                    return false;
                }

                if (!process_escaped_string(&c)) {
                    context->report_lex_error({start, point}, "Undefined escape sequence `\\", c,
                                              "`");
                    c = 0;
                }
            }

            char value = c;

            if (!next_character(file_contents, &point, &c) || c != '\'') {
                context->report_lex_error({start, point}, "Unterminated character literal");
                return false;
            }

            *location = point;

            token_out->v.ch = value;
            token_out->type = Token::Character;
            break;
        }

        case '"': {
            ZoneScopedN("lex::next_token string");

            cz::String value = {};
            Location start = point;

            *location = point;
            while (1) {
                Location middle = point;

                if (!next_character(file_contents, &point, &c)) {
                    context->report_lex_error({start, point}, "Unterminated string");
                    value.drop(lexer->string_buffer_array.allocator());
                    return false;
                }

                if (c == '\\') {
                    if (!next_character(file_contents, &point, &c)) {
                        context->report_lex_error({start, point}, "Unterminated string");
                        value.drop(lexer->string_buffer_array.allocator());
                        return false;
                    }

                    if (!process_escaped_string(&c)) {
                        context->report_lex_error({middle, point}, "Undefined escape sequence `\\",
                                                  c, "`");
                        goto skip_char;
                    }
                } else if (c == '"') {
                    break;
                }

                value.reserve(lexer->string_buffer_array.allocator(), 1);
                value.push(c);

            skip_char:
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
                ZoneScopedN("lex::next_token identifier");

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

                token_out->type = Token::Identifier;
                value.realloc(lexer->identifier_buffer_array.allocator());
                token_out->v.identifier = Hashed_Str::from_str(value);

            end_parse_label:
                break;
            }

            if (isdigit(c)) {
                ZoneScopedN("lex::next_token number");

                uint64_t value = 0;
                while (1) {
                    value *= 10;
                    value += c - '0';

                    *location = point;
                    if (!next_character(file_contents, &point, &c)) {
                        c = 0;
                        break;
                    }
                    if (!isdigit(c)) {
                        break;
                    }
                }

                uint32_t suffix = 0;
                while (1) {
                    if (c == 'u' || c == 'U') {
                        suffix |= Integer_Suffix::Unsigned;
                        *location = point;
                        if (!next_character(file_contents, &point, &c)) {
                            c = 0;
                        }
                    } else if (c == 'l' || c == 'L') {
                        char f = c;
                        *location = point;
                        if (!next_character(file_contents, &point, &c)) {
                            c = 0;
                        }
                        if (c == f) {
                            suffix |= Integer_Suffix::LongLong;
                            *location = point;
                            if (!next_character(file_contents, &point, &c)) {
                                c = 0;
                            }
                        } else {
                            suffix |= Integer_Suffix::Long;
                        }
                    } else {
                        break;
                    }
                }

                point = *location;
                token_out->v.integer.value = value;
                token_out->v.integer.suffix = suffix;
                token_out->type = Token::Integer;
                break;
            }

            context->report_lex_error({*location, point}, "Unable to process character `", c, "`");
            return false;
    }

    token_out->span.end = point;
    *location = point;
    return true;
}

}
}
