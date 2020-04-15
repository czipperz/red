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
    *out = file_contents.get(location->index);

switch_:
    switch (*out) {
        case File_Contents::eof:
            return false;

        case '\\': {
            if (file_contents.get(location->index + 1) == '\n') {
                location->index += 2;
                ++location->line;
                location->column = 0;
                goto top;
            }
            goto default_;
        }

        case '?': {
            if (file_contents.get(location->index + 1) == '?') {
                switch (file_contents.get(location->index + 2)) {
                    case '=':
                        *out = '#';
                        break;
                    case '/':
                        *out = '\\';
                        location->index += 2;
                        location->column += 2;
                        goto switch_;
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

                location->index += 3;
                location->column += 3;

                return true;
            }
            goto default_;
        }

        case '\n':
            ++location->index;
            ++location->line;
            location->column = 0;
            return true;

        default:
        default_:
            ++location->index;
            ++location->column;
            return true;
    }
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

#define IDENTIFIER_START_CASES \
    case 'a':                  \
    case 'b':                  \
    case 'c':                  \
    case 'd':                  \
    case 'e':                  \
    case 'f':                  \
    case 'g':                  \
    case 'h':                  \
    case 'i':                  \
    case 'j':                  \
    case 'k':                  \
    case 'l':                  \
    case 'm':                  \
    case 'n':                  \
    case 'o':                  \
    case 'p':                  \
    case 'q':                  \
    case 'r':                  \
    case 's':                  \
    case 't':                  \
    case 'u':                  \
    case 'v':                  \
    case 'w':                  \
    case 'x':                  \
    case 'y':                  \
    case 'z':                  \
    case 'A':                  \
    case 'B':                  \
    case 'C':                  \
    case 'D':                  \
    case 'E':                  \
    case 'F':                  \
    case 'G':                  \
    case 'H':                  \
    case 'I':                  \
    case 'J':                  \
    case 'K':                  \
    case 'L':                  \
    case 'M':                  \
    case 'N':                  \
    case 'O':                  \
    case 'P':                  \
    case 'Q':                  \
    case 'R':                  \
    case 'S':                  \
    case 'T':                  \
    case 'U':                  \
    case 'V':                  \
    case 'W':                  \
    case 'X':                  \
    case 'Y':                  \
    case 'Z':                  \
    case '_'

#define IDENTIFIER_MIDDLE_CASES \
    IDENTIFIER_START_CASES:     \
    NUMBER_CASES

#define NUMBER_CASES \
    case '0':        \
    case '1':        \
    case '2':        \
    case '3':        \
    case '4':        \
    case '5':        \
    case '6':        \
    case '7':        \
    case '8':        \
    case '9'

static void append_buffer_contents_multi_chunk_slice(cz::String* value,
                                                     const File_Contents& file_contents,
                                                     size_t start_base,
                                                     size_t start_offset,
                                                     size_t end_base,
                                                     size_t end_offset) {
    value->append({file_contents.buffers[start_base] + start_offset,
                   File_Contents::buffer_size - start_offset});

    // In the vast majority of cases there will be no chunks in the middle, but it is conceivably
    // possible so we handle it.  Maybe we just disallow stupidly long identifiers?
    for (size_t base = start_base + 1; base < end_base; ++base) {
        value->append({file_contents.buffers[base], File_Contents::buffer_size});
    }

    value->append({file_contents.buffers[end_base], end_offset});
}

static void append_buffer_contents_slice(cz::String* value,
                                         const File_Contents& file_contents,
                                         size_t start,
                                         size_t end) {
    size_t start_base = file_contents.get_base(start);
    size_t end_base = file_contents.get_base(end);
    size_t start_offset = file_contents.get_offset(start);
    size_t end_offset = file_contents.get_offset(end);
    if (start_base == end_base) {
        // We are in the same chunk of the file so we can just take a slice of an existing
        // string.  This will happen 99% of the time.
        value->append(
            {file_contents.buffers[start_base] + start_offset, end_offset - start_offset});
    } else {
        // We are over multiple chunks.
        append_buffer_contents_multi_chunk_slice(value, file_contents, start_base, start_offset,
                                                 end_base, end_offset);
    }
}

static void next_token_identifier(Lexer* lexer,
                                  const File_Contents& file_contents,
                                  Location* location,
                                  Location& point,
                                  char c,
                                  Token* token_out) {
    ZoneScoped;

    Location start = *location;

    while (1) {
        switch (file_contents.get(point.index)) {
        IDENTIFIER_MIDDLE_CASES:
            ++point.index;
            break;

            case '\\':
                if (file_contents.get(point.index + 1) == '\n') {
                    goto expensive_loop_backslash_newline;
                }
                goto commit_cheap_identifier;

            case '?':
                if (file_contents.get(point.index + 1) == '?' &&
                    file_contents.get(point.index + 2) == '/' &&
                    file_contents.get(point.index + 3) == '\n') {
                    goto expensive_loop_trigraph_backslash_newline;
                }
                goto commit_cheap_identifier;

            default:
                goto commit_cheap_identifier;
        }
    }

commit_cheap_identifier : {
    token_out->type = Token::Identifier;
    point.column += point.index - location->index - 1;
    size_t start_base = file_contents.get_base(location->index);
    size_t end_base = file_contents.get_base(point.index);
    size_t start_offset = file_contents.get_offset(location->index);
    size_t end_offset = file_contents.get_offset(point.index);
    if (start_base == end_base) {
        // We are in the same chunk of the file so we can just take a slice of an existing
        // string.  This will happen 99% of the time.
        token_out->v.identifier = Hashed_Str::from_str(
            {file_contents.buffers[start_base] + start_offset, end_offset - start_offset});
    } else {
        // We are over multiple chunks.  In the vast majority of cases there will be no
        // chunks in the middle, but it is conceivably possible so we handle it.
        cz::String value = {};
        value.reserve(lexer->identifier_buffer_array.allocator(), point.index - location->index);

        append_buffer_contents_multi_chunk_slice(&value, file_contents, start_base, start_offset,
                                                 end_base, end_offset);

        token_out->v.identifier = Hashed_Str::from_str(value);
    }
    return;
}

    cz::String value;
    if (0) {
    expensive_loop_trigraph_backslash_newline:
        value = {};
    continue_expensive_loop_trigraph_backslash_newline:
        value.reserve(lexer->identifier_buffer_array.allocator(), point.index - start.index);

        append_buffer_contents_slice(&value, file_contents, start.index, point.index);

        point.index += 4;
    } else {
    expensive_loop_backslash_newline:
        value = {};
    continue_expensive_loop_backslash_newline:
        value.reserve(lexer->identifier_buffer_array.allocator(), point.index - start.index);

        append_buffer_contents_slice(&value, file_contents, start.index, point.index);

        point.index += 2;
    }

    point.column = 0;
    ++point.line;
    start = point;

    while (1) {
        switch (file_contents.get(point.index)) {
        IDENTIFIER_MIDDLE_CASES:
            ++point.index;
            break;

            case '\\':
                if (file_contents.get(point.index + 1) == '\n') {
                    goto continue_expensive_loop_backslash_newline;
                }
                goto commit_expensive_identifier;

            case '?':
                if (file_contents.get(point.index + 1) == '?' &&
                    file_contents.get(point.index + 2) == '/' &&
                    file_contents.get(point.index + 3) == '\n') {
                    goto continue_expensive_loop_trigraph_backslash_newline;
                }
                goto commit_expensive_identifier;

            default:
                goto commit_expensive_identifier;
        }
    }

commit_expensive_identifier:
    token_out->type = Token::Identifier;
    point.column += point.index - location->index;
    value.reserve(lexer->identifier_buffer_array.allocator(), point.index - start.index);
    append_buffer_contents_slice(&value, file_contents, start.index, point.index);
    value.realloc(lexer->identifier_buffer_array.allocator());
    token_out->v.identifier = Hashed_Str::from_str(value);
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
                } else if (next == '<') {
                    *location = point;
                    if (next_character(file_contents, &point, &next) && next == '=') {
                        token_out->type = Token::LeftShiftSet;
                    } else {
                        token_out->type = Token::LeftShift;
                        point = *location;
                    }
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
            if (next_character(file_contents, &point, &next)) {
                if (next == '=') {
                    token_out->type = Token::GreaterEqual;
                } else if (next == '>') {
                    *location = point;
                    if (next_character(file_contents, &point, &next) && next == '=') {
                        token_out->type = Token::RightShiftSet;
                    } else {
                        token_out->type = Token::RightShift;
                        point = *location;
                    }
                } else {
                    token_out->type = Token::GreaterThan;
                    point = *location;
                }
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
            if (next_character(file_contents, &point, &next)) {
                if (next == '>') {
                    token_out->type = Token::CloseCurly;
                } else if (next == '=') {
                    token_out->type = Token::ModulusSet;
                } else {
                    token_out->type = Token::Ampersand;
                    point = *location;
                }
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
        case '+': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::PlusSet;
            } else {
                token_out->type = Token::Plus;
                point = *location;
            }
            break;
        }
        case '-': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::MinusSet;
            } else {
                token_out->type = Token::Minus;
                point = *location;
            }
            break;
        }
        case '/': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next)) {
                if (next == '*') {
                    ZoneScopedN("lex::next_token block comment");
                    size_t start_index = point.index - point.column;
                    while (1) {
                    block_comment_switch:
                        switch (file_contents.get(point.index)) {
                            case '*':
                                ++point.index;
                                while (1) {
                                    switch (file_contents.get(point.index)) {
                                        case '*':
                                            ++point.index;
                                            continue;

                                        case File_Contents::eof:
                                            point.column = point.index - start_index;
                                            context->report_lex_error({*location, point},
                                                                      "Unterminated block comment");
                                            *location = point;
                                            return false;

                                        case '/':
                                            ++point.index;
                                            point.column = point.index - start_index;
                                            *location = point;
                                            goto top;

                                        case '\\':
                                            if (file_contents.get(point.index + 1) == '\n') {
                                                point.index += 2;
                                                ++point.line;
                                                start_index = point.index;
                                                continue;
                                            }
                                            goto block_comment_switch;

                                        case '?':
                                            if (file_contents.get(point.index + 1) == '?' &&
                                                file_contents.get(point.index + 2) == '/' &&
                                                file_contents.get(point.index + 3) == '\n') {
                                                point.index += 4;
                                                ++point.line;
                                                start_index = point.index;
                                                continue;
                                            }
                                            goto block_comment_switch;

                                        default:
                                            goto block_comment_switch;
                                    }
                                }

                            case File_Contents::eof:
                                point.column = point.index - start_index;
                                context->report_lex_error({*location, point},
                                                          "Unterminated block comment");
                                *location = point;
                                return false;

                            case '\n':
                                ++point.index;
                                ++point.line;
                                start_index = point.index;
                                break;

                            default:
                                ++point.index;
                                break;
                        }
                    }
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
                } else if (next == '=') {
                    token_out->type = Token::DivideSet;
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
        case '*': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::MultiplySet;
            } else {
                token_out->type = Token::Star;
                point = *location;
            }
            break;
        }
        case '&': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next)) {
                if (next == '&') {
                    token_out->type = Token::And;
                } else if (next == '=') {
                    token_out->type = Token::BitAndSet;
                } else {
                    token_out->type = Token::Ampersand;
                    point = *location;
                }
            } else {
                token_out->type = Token::Ampersand;
                point = *location;
            }
            break;
        }
        case '|': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next)) {
                if (next == '|') {
                    token_out->type = Token::Or;
                } else if (next == '=') {
                    token_out->type = Token::BitOrSet;
                } else {
                    token_out->type = Token::Pipe;
                    point = *location;
                }
            } else {
                token_out->type = Token::Pipe;
                point = *location;
            }
            break;
        }
        case '^': {
            *location = point;
            char next;
            if (next_character(file_contents, &point, &next) && next == '=') {
                token_out->type = Token::BitXorSet;
            } else {
                token_out->type = Token::Xor;
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

        IDENTIFIER_START_CASES:
            next_token_identifier(lexer, file_contents, location, point, c, token_out);
            break;

        NUMBER_CASES : {
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

        default:
            context->report_lex_error({*location, point}, "Unable to process character `", c, "`");
            return false;
    }

    token_out->span.end = point;
    *location = point;
    return true;
}

}
}
