#include "preprocess.hpp"

#include <ctype.h>
#include <cz/assert.hpp>
#include <cz/log.hpp>
#include <cz/path.hpp>
#include "load.hpp"

namespace red {
namespace cpp {

char next_character(const FileBuffer& file_buffer, Location* location) {
    char buffer[3];
top:
    buffer[0] = file_buffer.get(location->index);
    if (buffer[0] == '\0') {
        return buffer[0];
    }

    if (buffer[0] == '?') {
        buffer[1] = file_buffer.get(location->index + 1);
        if (buffer[1] == '?') {
            buffer[2] = file_buffer.get(location->index + 2);
            switch (buffer[2]) {
                case '=':
                    buffer[0] = '#';
                    break;
                case '/':
                    buffer[0] = '\\';
                    break;
                case '\'':
                    buffer[0] = '^';
                    break;
                case '(':
                    buffer[0] = '[';
                    break;
                case ')':
                    buffer[0] = ']';
                    break;
                case '!':
                    buffer[0] = '|';
                    break;
                case '<':
                    buffer[0] = '{';
                    break;
                case '>':
                    buffer[0] = '}';
                    break;
                case '-':
                    buffer[0] = '~';
                    break;
                default:
                    ++location->index;
                    return '?';
            }
            location->index += 2;
            location->column += 2;
        }
    }

    buffer[1] = file_buffer.get(location->index + 1);
    if (buffer[0] == '\\' && buffer[1] == '\n') {
        location->index += 2;
        ++location->line;
        location->column = 0;
        goto top;
    }

    ++location->index;
    if (buffer[0] == '\n') {
        ++location->line;
        location->column = 0;
    } else {
        ++location->column;
    }

    return buffer[0];
}

bool next_token(C* ctxt,
                const FileBuffer& file_buffer,
                Location* location,
                Token* token_out,
                bool* at_bol,
                cz::AllocatedString* label_value) {
    Location point = *location;
    bool next_at_bol = *at_bol;
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
        case '&':
            *location = point;
            c = next_character(file_buffer, &point);
            if (c == '&') {
                *location = point;
                token_out->type = Token::And;
            } else {
                token_out->type = Token::Ampersand;
            }
            break;
        case '|':
            *location = point;
            c = next_character(file_buffer, &point);
            if (c == '|') {
                *location = point;
                token_out->type = Token::Or;
            } else {
                token_out->type = Token::Pipe;
            }
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
            label_value->set_len(0);

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
                    next_at_bol = true;
                }
                *location = point;
                goto top;
            }

            if (isalpha(c) || c == '_') {
                label_value->set_len(0);

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

                if (*label_value == "auto") {
                    token_out->type = Token::Auto;
                } else if (*label_value == "break") {
                    token_out->type = Token::Break;
                } else if (*label_value == "case") {
                    token_out->type = Token::Case;
                } else if (*label_value == "char") {
                    token_out->type = Token::Char;
                } else if (*label_value == "const") {
                    token_out->type = Token::Const;
                } else if (*label_value == "continue") {
                    token_out->type = Token::Continue;
                } else if (*label_value == "default") {
                    token_out->type = Token::Default;
                } else if (*label_value == "do") {
                    token_out->type = Token::Do;
                } else if (*label_value == "double") {
                    token_out->type = Token::Double;
                } else if (*label_value == "else") {
                    token_out->type = Token::Else;
                } else if (*label_value == "enum") {
                    token_out->type = Token::Enum;
                } else if (*label_value == "extern") {
                    token_out->type = Token::Extern;
                } else if (*label_value == "float") {
                    token_out->type = Token::Float;
                } else if (*label_value == "for") {
                    token_out->type = Token::For;
                } else if (*label_value == "goto") {
                    token_out->type = Token::Goto;
                } else if (*label_value == "if") {
                    token_out->type = Token::If;
                } else if (*label_value == "int") {
                    token_out->type = Token::Int;
                } else if (*label_value == "long") {
                    token_out->type = Token::Long;
                } else if (*label_value == "register") {
                    token_out->type = Token::Register;
                } else if (*label_value == "return") {
                    token_out->type = Token::Return;
                } else if (*label_value == "short") {
                    token_out->type = Token::Short;
                } else if (*label_value == "signed") {
                    token_out->type = Token::Signed;
                } else if (*label_value == "sizeof") {
                    token_out->type = Token::Sizeof;
                } else if (*label_value == "static") {
                    token_out->type = Token::Static;
                } else if (*label_value == "struct") {
                    token_out->type = Token::Struct;
                } else if (*label_value == "switch") {
                    token_out->type = Token::Switch;
                } else if (*label_value == "typedef") {
                    token_out->type = Token::Typedef;
                } else if (*label_value == "union") {
                    token_out->type = Token::Union;
                } else if (*label_value == "unsigned") {
                    token_out->type = Token::Unsigned;
                } else if (*label_value == "void") {
                    token_out->type = Token::Void;
                } else if (*label_value == "volatile") {
                    token_out->type = Token::Volatile;
                } else if (*label_value == "while") {
                    token_out->type = Token::While;
                } else {
                    token_out->type = Token::Label;
                }

                break;
            }

            if (isdigit(c)) {
                label_value->set_len(0);

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
    *at_bol = next_at_bol;
    return true;
}

void Preprocessor::destroy(C* c) {
    file_pragma_once.drop(c->allocator);
    include_stack.drop(c->allocator);

    for (size_t i = 0; i < definitions.cap(); ++i) {
        Definition* definition = definitions.get_index(i);
        if (definition) {
            definition->drop(c->allocator);
        }
    }
    definitions.drop(c->allocator);
}

Location Preprocessor::location() const {
    return include_stack.last().location;
}

static void advance_over_whitespace(const FileBuffer& buffer, Location* location) {
    Location point = *location;
    while (isspace(next_character(buffer, &point))) {
        *location = point;
    }
}

static Result read_include(const FileBuffer& buffer,
                           Location* location,
                           Location* end_out,
                           char target,
                           cz::AllocatedString* output) {
    while (1) {
        *end_out = *location;
        char c = next_character(buffer, location);
        if (c == target) {
            return Result::ok();
        }

        if (!c) {
            return {Result::ErrorEndOfFile};
        }

        output->reserve(1);
        output->push(c);
    }
}

static Result process_include(C* c,
                              Preprocessor* p,
                              Token* token_out,
                              cz::AllocatedString* label_value) {
    Location* point = &p->include_stack.last().location;
    advance_over_whitespace(c->files.buffers[point->file], point);

    Location backup = *point;
    char ch = next_character(c->files.buffers[point->file], point);
    if (ch != '<' && ch != '"') {
        *point = backup;
        CZ_PANIC("Unimplemented #include macro");
    }

    Span included_span;
    included_span.start = *point;
    label_value->set_len(0);
    CZ_TRY(read_include(c->files.buffers[point->file], point, &included_span.end,
                        ch == '<' ? '>' : '"', label_value));

    cz::AllocatedString file_name;
    // @TODO: Use a multi arena allocator here instead of fragmenting it.
    file_name.allocator = c->allocator;

    for (size_t i = c->options.include_paths.len() + 1; i > 0; --i) {
        cz::Str include_path;
        if (i == c->options.include_paths.len() + 1) {
            // try local directory
            if (ch == '"') {
                include_path = cz::path::directory_component(c->files.names[point->file]);
            } else {
                continue;
            }
        } else {
            // @Speed: store the include paths as Str s so we don't call strlen
            // over and over here
            include_path = c->options.include_paths[i - 1];
        }

        bool trailing_slash = include_path.ends_with("/");  // @Speed: ends_with(char)
        file_name.set_len(0);
        file_name.reserve(include_path.len + !trailing_slash + label_value->len() + 1);
        file_name.append(include_path);
        if (!trailing_slash) {
            file_name.push('/');
        }
        file_name.append(*label_value);
        cz::path::flatten(&file_name);
        file_name.realloc_null_terminate();

        CZ_LOG(c, Trace, "Trying '", file_name, "'");

        if (load_file(c, p, file_name).is_ok()) {
            CZ_LOG(c, Debug, "Including '", c->files.names[p->location().file], '\'');
            return p->next(c, token_out, label_value);
        }
    }

    file_name.drop();
    c->report_error(included_span, "Couldn't include file '", *label_value, "'");
    return {Result::ErrorInvalidInput};
}

static Result process_token(C* c,
                            Preprocessor* p,
                            Token* token,
                            cz::AllocatedString* label_value,
                            bool at_bol);

static Result process_next(C* c,
                           Preprocessor* p,
                           Token* token_out,
                           cz::AllocatedString* label_value,
                           bool at_bol,
                           bool has_next);

static bool skip_until_eol(C* c,
                           Preprocessor* p,
                           Token* token_out,
                           cz::AllocatedString* label_value) {
    bool at_bol = false;
    Location* point = &p->include_stack.last().location;
    while (!at_bol &&
           next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
    }
    return at_bol;
}

#define SKIP_UNTIL_EOL                                                     \
    ([&]() {                                                               \
        bool at_bol = skip_until_eol(c, p, token_out, label_value);        \
        return process_next(c, p, token_out, label_value, at_bol, at_bol); \
    })

static Result process_pragma(C* c,
                             Preprocessor* p,
                             Token* token_out,
                             cz::AllocatedString* label_value) {
    Location* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
        // ignore #pragma
        return p->next(c, token_out, label_value);
    }

    if (at_bol) {
        // #pragma is ignored
        return process_token(c, p, token_out, label_value, at_bol);
    }

    if (token_out->type == Token::Label && *label_value == "once") {
        p->file_pragma_once[point->file] = true;

        at_bol = false;
        if (!next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
            // #pragma once \EOF
            return p->next(c, token_out, label_value);
        }

        if (!at_bol) {
            c->report_error(token_out->span, "#pragma once has trailing tokens");
            return SKIP_UNTIL_EOL();
        }

        // done processing the #pragma once so get the next token
        return process_token(c, p, token_out, label_value, at_bol);
    }

    c->report_error(token_out->span, "Unknown #pragma");
    return SKIP_UNTIL_EOL();
}

static Result process_if_true(C* c,
                              Preprocessor* p,
                              Token* token_out,
                              cz::AllocatedString* label_value) {
    Location* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
        c->report_error({*point, *point}, "Unterminated preprocessing branch");
        return {Result::ErrorInvalidInput};
    }

    return process_token(c, p, token_out, label_value, at_bol);
}

static Result process_if_false(C* c,
                               Preprocessor* p,
                               Token* token_out,
                               cz::AllocatedString* label_value) {
    size_t skip_depth = 0;
    while (1) {
        bool at_bol = skip_until_eol(c, p, token_out, label_value);
        if (!at_bol) {
            Location point = p->location();
            c->report_error({point, point}, "Unterminated #if");
            return {Result::ErrorInvalidInput};
        }

    check_hash:
        if (token_out->type == Token::Hash) {
            at_bol = false;
            IncludeInfo* info = &p->include_stack.last();
            if (!next_token(c, c->files.buffers[info->location.file], &info->location, token_out,
                            &at_bol, label_value)) {
                Location point = p->location();
                c->report_error({point, point}, "Unterminated #if");
                return {Result::ErrorInvalidInput};
            }

            if (at_bol) {
                goto check_hash;
            }

            if (token_out->type == Token::Label) {
                if (*label_value == "ifdef" && *label_value == "ifndef" && *label_value == "if") {
                    ++skip_depth;
                } else if (*label_value == "else") {
                    if (skip_depth == 0) {
                        break;
                    }
                } else if (*label_value == "endif") {
                    if (skip_depth > 0) {
                        --skip_depth;
                    } else {
                        CZ_DEBUG_ASSERT(info->if_depth > 0);
                        --info->if_depth;
                        break;
                    }
                }
            }
        }
    }

    return p->next(c, token_out, label_value);
}

static Result process_ifdef(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value,
                            bool want_present) {
    Span ifdef_span = token_out->span;

    IncludeInfo* point = &p->include_stack.last();
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value) ||
        at_bol) {
        c->report_error(ifdef_span, "No macro to test");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    if (token_out->type != Token::Label) {
        c->report_error(token_out->span, "Must test a macro name");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    point->if_depth++;

    if (!!p->definitions.find(*label_value) == want_present) {
        return process_if_true(c, p, token_out, label_value);
    } else {
        return process_if_false(c, p, token_out, label_value);
    }
}

static Result process_if(C* c,
                         Preprocessor* p,
                         Token* token_out,
                         cz::AllocatedString* label_value) {
    CZ_PANIC("Unimplemented #if");
}

static Result process_else(C* c,
                           Preprocessor* p,
                           Token* token_out,
                           cz::AllocatedString* label_value) {
    // We just produced x and are skipping over y
    // #if 1
    // x
    // |#else
    // y
    // #endif

    IncludeInfo* point = &p->include_stack.last();
    if (point->if_depth == 0) {
        c->report_error(token_out->span, "#else without #if");
        return {Result::ErrorInvalidInput};
    }

    return process_if_false(c, p, token_out, label_value);
}

static Result process_endif(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value) {
    IncludeInfo* point = &p->include_stack.last();
    if (point->if_depth == 0) {
        c->report_error(token_out->span, "#endif without #if");
        return {Result::ErrorInvalidInput};
    }

    --point->if_depth;
    return SKIP_UNTIL_EOL();
}

static Result process_define(C* c,
                             Preprocessor* p,
                             Token* token_out,
                             cz::AllocatedString* label_value) {
    Span define_span = token_out->span;

    IncludeInfo* point = &p->include_stack.last();
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value) ||
        at_bol) {
        c->report_error(define_span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    if (token_out->type != Token::Label) {
        c->report_error(token_out->span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    cz::String definition_name = label_value->clone(c->allocator);

    Definition definition;
    definition.is_function = false;

    at_bol = false;
    while (!at_bol && next_token(c, c->files.buffers[point->location.file], &point->location,
                                 token_out, &at_bol, label_value)) {
        definition.tokens.reserve(c->allocator, 1);
        definition.token_values.reserve(c->allocator, 1);
        cz::String val;
        if (token_out->type == Token::Label || token_out->type == Token::String ||
            token_out->type == Token::Integer) {
            val = label_value->clone(c->allocator);
        }
        definition.tokens.push(*token_out);
        definition.token_values.push(val);
    }

    p->definitions.reserve(c->allocator, 1);
    // consumes definition_name
    p->definitions.set(definition_name, c->allocator, definition);

    return process_next(c, p, token_out, label_value, at_bol, at_bol);
}

static Result process_undef(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value) {
    Span undef_span = token_out->span;

    IncludeInfo* point = &p->include_stack.last();
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value) ||
        at_bol) {
        c->report_error(undef_span, "Must specify the macro to undefine");
        return SKIP_UNTIL_EOL();
    }

    if (token_out->type != Token::Label) {
        c->report_error(token_out->span, "Must specify the macro to undefine");
        return SKIP_UNTIL_EOL();
    }

    p->definitions.remove(*label_value, c->allocator);

    return SKIP_UNTIL_EOL();
}

static Result process_error(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value) {
    c->report_error(token_out->span, "Explicit error");
    return SKIP_UNTIL_EOL();
}

static Result process_label(C* c, Preprocessor* p, Token* token, cz::AllocatedString* label_value) {
    Definition* definition = p->definitions.find(*label_value);
    if (definition) {
        p->definition_stack.reserve(c->allocator, 1);
        DefinitionInfo info;
        info.definition = definition;
        info.index = 0;
        p->definition_stack.push(info);
        oy
    } else {
        return Result::ok();
    }
}

static Result process_token(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value,
                            bool at_bol) {
top:
    Location* point = &p->include_stack.last().location;
    if (at_bol && token_out->type == Token::Hash) {
        at_bol = false;
        if (next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
            if (at_bol) {
                // #\n is ignored
                goto top;
            }

            if (token_out->type == Token::Label) {
                if (*label_value == "include") {
                    return process_include(c, p, token_out, label_value);
                }
                if (*label_value == "pragma") {
                    return process_pragma(c, p, token_out, label_value);
                }
                if (*label_value == "ifdef") {
                    return process_ifdef(c, p, token_out, label_value, true);
                }
                if (*label_value == "ifndef") {
                    return process_ifdef(c, p, token_out, label_value, false);
                }
                if (*label_value == "endif") {
                    return process_endif(c, p, token_out, label_value);
                }
                if (*label_value == "define") {
                    return process_define(c, p, token_out, label_value);
                }
                if (*label_value == "undef") {
                    return process_undef(c, p, token_out, label_value);
                }
                if (*label_value == "error") {
                    return process_error(c, p, token_out, label_value);
                }
            } else if (token_out->type == Token::If) {
                return process_if(c, p, token_out, label_value);
            } else if (token_out->type == Token::Else) {
                return process_else(c, p, token_out, label_value);
            }

            c->report_error(token_out->span, "Unknown preprocessor attribute");
            return SKIP_UNTIL_EOL();
        }
    } else if (token_out->type == Token::Label) {
        return process_label(c, p, token_out, label_value);
    }

    return Result::ok();
}

static Result process_next(C* c,
                           Preprocessor* p,
                           Token* token_out,
                           cz::AllocatedString* label_value,
                           bool at_bol,
                           bool has_next) {
    if (has_next) {
        return process_token(c, p, token_out, label_value, at_bol);
    } else {
        Location* point = &p->include_stack.last().location;
        if (point->index < c->files.buffers[point->file].len()) {
            Location end = *point;
            next_character(c->files.buffers[point->file], &end);
            c->report_error({*point, end}, "Invalid input '",
                            c->files.buffers[point->file].get(point->index), "'");
            return {Result::ErrorInvalidInput};
        } else {
            CZ_DEBUG_ASSERT(point->index == c->files.buffers[point->file].len());
            return p->next(c, token_out, label_value);
        }
    }
}

Result Preprocessor::next(C* c, Token* token_out, cz::AllocatedString* label_value) {
    if (include_stack.len() == 0) {
        return Result::done();
    }

    Location* point = &include_stack.last().location;
    while (point->index == c->files.buffers[point->file].len()) {
        auto entry = include_stack.pop();

        if (entry.if_depth > 0) {
            c->report_error({}, "Unterminated #if");
        }

        if (include_stack.len() == 0) {
            return Result::done();
        }

        point = &include_stack.last().location;
    }

    bool at_bol = point->index == 0;
    bool has_next =
        next_token(c, c->files.buffers[point->file], point, token_out, &at_bol, label_value);
    return process_next(c, this, token_out, label_value, at_bol, has_next);
}

}
}
