#include "preprocess.hpp"

#include <ctype.h>
#include <cz/assert.hpp>
#include <cz/fs/directory.hpp>
#include <cz/log.hpp>
#include "text_replacement.hpp"

namespace red {

void Preprocessor::push(C* c, const char* file_name, FileBuffer file_buffer) {
    c->files.buffers.reserve(c->allocator, 1);
    c->files.names.reserve(c->allocator, 1);
    file_pragma_once.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    size_t file = c->files.buffers.len();
    c->files.buffers.push(file_buffer);
    c->files.names.push(file_name);
    file_pragma_once.push(false);
    Location location = {};
    location.file = file;
    include_stack.push({location});
}

void Preprocessor::destroy(C* c) {
    file_pragma_once.drop(c->allocator);
    include_stack.drop(c->allocator);

    for (size_t i = 0; i < definitions.cap(); ++i) {
        Definition* definition = definitions.get_index(i);
        if (definition) {
            definition->tokens.drop(c->allocator);

            for (size_t j = 0; j < definition->token_values.len(); ++j) {
                definition->token_values[j].drop(c->allocator);
            }
            definition->token_values.drop(c->allocator);
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

    cz::AllocatedString file_name;
    // @TODO: Use a multi arena allocator here instead of fragmenting it.
    file_name.allocator = c->allocator;

    if (ch == '"') {
        cz::Str directory_name = cz::fs::directory_component(c->files.names[point->file]);
        file_name.reserve(directory_name.len);
        file_name.append(directory_name);
    }
    size_t offset = file_name.len();

    Span included_span;
    included_span.start = *point;
    CZ_TRY(read_include(c->files.buffers[point->file], point, &included_span.end,
                        ch == '<' ? '>' : '"', &file_name));

    CZ_LOG(c, Debug, "Including '", cz::Str{file_name.buffer() + offset, file_name.len() - offset},
           '\'');

    FileBuffer file_buffer;
    if (ch == '"') {
        CZ_LOG(c, Trace, "Trying '", file_name, "'");
        file_name.reserve(1);
        *file_name.end() = '\0';

        // these allocators are probably going to change
        auto result = file_buffer.read(file_name.buffer(), c->allocator);
        if (result.is_ok()) {
            CZ_LOG(c, Trace, "Contents: \n", file_buffer);
        } else {
            CZ_DEBUG_ASSERT(file_buffer.len() == 0);
        }
    }

    if (file_buffer.len() == 0) {
        cz::Str included_file_name = {file_name.buffer() + offset, file_name.len() - offset};

        cz::AllocatedString temp;
        temp.allocator = file_name.allocator;

        for (size_t i = c->options.include_paths.len(); file_buffer.len() == 0 && i > 0; --i) {
            // @Speed: store the include paths as Str s so we don't call strlen
            // over and over here
            cz::Str include_path(c->options.include_paths[i - 1]);
            bool trailing_slash = include_path.ends_with("/");  // @Speed: ends_with(char)
            temp.reserve(include_path.len + !trailing_slash + included_file_name.len + 1);
            temp.append(include_path);
            if (!trailing_slash) {
                temp.push('/');
            }
            temp.append(included_file_name);
            *temp.end() = '\0';

            CZ_LOG(c, Trace, "Trying '", temp, "'");

            // these allocators are probably going to change
            auto result = file_buffer.read(temp.buffer(), c->allocator);
            if (result.is_ok()) {
                CZ_LOG(c, Trace, "Contents: \n", file_buffer);
                file_name.drop();
                file_name = temp;
                break;
            } else {
                temp.clear();
                CZ_DEBUG_ASSERT(file_buffer.len() == 0);
            }
        }

        if (file_buffer.len() == 0) {
            temp.drop();
            c->report_error(included_span, "Couldn't include file '", included_file_name, "'");
            return {Result::ErrorInvalidInput};
        }
    }

    p->push(c, file_name.buffer(), file_buffer);

    return p->next(c, token_out, label_value);
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

    Location start = token_out->span.start;
    at_bol = skip_until_eol(c, p, token_out, label_value);
    // TODO: make this just report for the token
    c->report_error({start, token_out->span.end}, "Unknown #pragma");
    return process_next(c, p, token_out, label_value, at_bol, at_bol);
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
            CZ_PANIC("Unimplemented unterminated #if");
        }

        if (token_out->type == Token::Hash) {
            Location* point = &p->include_stack.last().location;
            if (!next_token(c, c->files.buffers[point->file], point, token_out, &at_bol,
                            label_value)) {
                CZ_PANIC("Unimplemented unterminated #if");
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
                        break;
                    }
                }
            }
        }
    }

    return Result::ok();
}

template <bool want_present>
static Result process_ifdef(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value) {
    IncludeInfo* point = &p->include_stack.last();
    Location backup = point->location;
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value) ||
        at_bol) {
        // TODO fix this position
        c->report_error({backup, point->location}, "No macro to test");
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

    if (p->definitions.find(*label_value).is_present() == want_present) {
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
        c->report_error(token_out->span, "#else without #if");
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
    Location backup = point->location;
    bool at_bol = false;
    if (!next_token(c, c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value)) {
        c->report_error(define_span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    if (at_bol) {
        // TODO merge with case above
        c->report_error({backup, point->location}, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    if (token_out->type != Token::Label) {
        c->report_error(token_out->span, "Must give the macro a name");
        return SKIP_UNTIL_EOL();
    }

    cz::String definition_name = label_value->clone(c->allocator);
    CZ_DEFER(definition_name.drop(c->allocator));

    Definition definition;
    definition.is_function = false;

    at_bol = false;
    while (!at_bol && next_token(c, c->files.buffers[point->location.file], &point->location,
                                 token_out, &at_bol, label_value)) {
        definition.tokens.reserve(c->allocator, 1);
        definition.token_values.reserve(c->allocator, 1);
        cz::String val;
        if (token_out->type == Token::Label && token_out->type == Token::String &&
            token_out->type == Token::Integer) {
            val = label_value->clone(c->allocator);
        }
        definition.tokens.push(*token_out);
        definition.token_values.push(val);
    }

    p->definitions.reserve(c->allocator, 1);
    auto entry = p->definitions.find(definition_name);
    entry.set(c->allocator, definition);
    return process_next(c, p, token_out, label_value, at_bol, at_bol);
}

static Result process_error(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::AllocatedString* label_value) {
    c->report_error(token_out->span, "Explicit error");
    return SKIP_UNTIL_EOL();
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
                    return process_ifdef<true>(c, p, token_out, label_value);
                }
                if (*label_value == "ifndef") {
                    return process_ifdef<false>(c, p, token_out, label_value);
                }
                if (*label_value == "if") {
                    return process_if(c, p, token_out, label_value);
                }
                if (*label_value == "else") {
                    return process_else(c, p, token_out, label_value);
                }
                if (*label_value == "endif") {
                    return process_endif(c, p, token_out, label_value);
                }
                if (*label_value == "define") {
                    return process_define(c, p, token_out, label_value);
                }
                if (*label_value == "error") {
                    return process_error(c, p, token_out, label_value);
                }
            }

            c->report_error(token_out->span, "Unknown preprocessor attribute");
            return SKIP_UNTIL_EOL();
        }
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
        include_stack.pop();

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
