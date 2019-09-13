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
                           cz::Allocated<cz::String>* output) {
    while (1) {
        *end_out = *location;
        char c = next_character(buffer, location);
        if (c == target) {
            return Result::ok();
        }

        if (!c) {
            return {Result::ErrorEndOfFile};
        }

        output->object.reserve(output->allocator, 1);
        output->object.push(c);
    }
}

static Result process_include(C* c,
                              Preprocessor* p,
                              Token* token_out,
                              cz::Allocated<cz::String>* label_value) {
    Location* point = &p->include_stack.last().location;
    advance_over_whitespace(c->files.buffers[point->file], point);

    Location backup = *point;
    char ch = next_character(c->files.buffers[point->file], point);
    if (ch != '<' && ch != '"') {
        *point = backup;
        CZ_PANIC("Unimplemented #include macro");
    }

    cz::Allocated<cz::String> file_name;
    // @TODO: Use a multi arena allocator here instead of fragmenting it.
    file_name.allocator = c->allocator;

    if (ch == '"') {
        cz::Str directory_name = cz::fs::directory_component(c->files.names[point->file]);
        file_name.object.reserve(file_name.allocator, directory_name.len);
        file_name.object.append(directory_name);
    }
    size_t offset = file_name.object.len();

    Location start = *point;
    Location end;
    CZ_TRY(read_include(c->files.buffers[point->file], point, &end, ch == '<' ? '>' : '"',
                        &file_name));

    CZ_LOG(c, Debug, "Including '",
           cz::Str{file_name.object.buffer() + offset, file_name.object.len() - offset}, '\'');

    FileBuffer file_buffer;
    if (ch == '"') {
        CZ_LOG(c, Trace, "Trying '", file_name.object, "'");
        file_name.object.reserve(file_name.allocator, 1);
        *file_name.object.end() = '\0';

        // these allocators are probably going to change
        auto result = file_buffer.read(file_name.object.buffer(), c->allocator, c->allocator);
        if (result.is_ok()) {
            CZ_LOG(c, Trace, "Contents: \n", file_buffer);
        } else {
            CZ_DEBUG_ASSERT(file_buffer.len() == 0);
        }
    }

    if (file_buffer.len() == 0) {
        cz::Str included_file_name = {file_name.object.buffer() + offset,
                                      file_name.object.len() - offset};

        cz::Allocated<cz::String> temp;
        temp.allocator = file_name.allocator;

        for (size_t i = c->options.include_paths.len(); file_buffer.len() == 0 && i > 0; --i) {
            // @Speed: store the include paths as Str s so we don't call strlen
            // over and over here
            cz::Str include_path(c->options.include_paths[i - 1]);
            bool trailing_slash = include_path.ends_with("/");  // @Speed: ends_with(char)
            temp.object.reserve(temp.allocator,
                                include_path.len + !trailing_slash + included_file_name.len + 1);
            temp.object.append(include_path);
            if (!trailing_slash) {
                temp.object.push('/');
            }
            temp.object.append(included_file_name);
            *temp.object.end() = '\0';

            CZ_LOG(c, Trace, "Trying '", temp.object, "'");

            // these allocators are probably going to change
            auto result = file_buffer.read(temp.object.buffer(), c->allocator, c->allocator);
            if (result.is_ok()) {
                CZ_LOG(c, Trace, "Contents: \n", file_buffer);
                file_name.object.drop(file_name.allocator);
                file_name = temp;
                break;
            } else {
                temp.object.clear();
                CZ_DEBUG_ASSERT(file_buffer.len() == 0);
            }
        }

        if (file_buffer.len() == 0) {
            temp.object.drop(temp.allocator);
            c->report_error(point->file, start, end, "Couldn't include file '", included_file_name,
                            "'");
            return {Result::ErrorInvalidInput};
        }
    }

    p->push(c, file_name.object.buffer(), file_buffer);

    return p->next(c, token_out, label_value);
}

static Result process_token(C* c,
                            Preprocessor* p,
                            Token* token,
                            cz::Allocated<cz::String>* label_value,
                            bool at_bol);

static Result process_pragma(C* c,
                             Preprocessor* p,
                             Token* token_out,
                             cz::Allocated<cz::String>* label_value) {
    Location* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
        // ignore #pragma
        return p->next(c, token_out, label_value);
    }

    if (at_bol) {
        // #pragma is ignored
        return process_token(c, p, token_out, label_value, at_bol);
    }

    if (token_out->type == Token::Label && label_value->object == "once") {
        p->file_pragma_once[point->file] = true;

        at_bol = false;
        if (!next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
            // #pragma once \EOF
            return p->next(c, token_out, label_value);
        }

        if (!at_bol) {
            c->report_error(point->file, token_out->start, token_out->end,
                            "#pragma once has trailing tokens");

            // eat until eof
            at_bol = false;
            while (!at_bol && next_token(c->files.buffers[point->file], point, token_out, &at_bol,
                                         label_value)) {
            }
        }

        // done processing the #pragma once so get the next token
        return process_token(c, p, token_out, label_value, at_bol);
    }

    auto start = token_out->start;

    // eat until eof
    at_bol = false;
    while (!at_bol &&
           next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
    }

    c->report_error(point->file, start, token_out->end, "Unknown #pragma");

    return process_token(c, p, token_out, label_value, at_bol);
}

static Result process_if_true(C* c,
                              Preprocessor* p,
                              Token* token_out,
                              cz::Allocated<cz::String>* label_value) {
    Location* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
        c->report_error(point->file, *point, *point, "Unterminated preprocessing branch");
        return {Result::ErrorInvalidInput};
    }

    return process_token(c, p, token_out, label_value, at_bol);
}

static Result process_if_false(C* c,
                               Preprocessor* p,
                               Token* token_out,
                               cz::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

template <bool want_present>
static Result process_ifdef(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::Allocated<cz::String>* label_value) {
    Location ifdef_start = token_out->start;
    Location ifdef_end = token_out->end;

    IncludeInfo* point = &p->include_stack.last();
    Location backup = point->location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value) ||
        at_bol) {
        c->report_error(point->location.file, backup, point->location, "No macro to test");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    if (token_out->type != Token::Label) {
        c->report_error(point->location.file, backup, point->location, "Must test a macro name");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    point->if_depth++;

    if (p->definitions.find(label_value->object).is_present() == want_present) {
        return process_if_true(c, p, token_out, label_value);
    } else {
        return process_if_false(c, p, token_out, label_value);
    }
}

static Result process_else(C* c,
                           Preprocessor* p,
                           Token* token_out,
                           cz::Allocated<cz::String>* label_value) {
    IncludeInfo* point = &p->include_stack.last();
    if (point->if_skip_depth > 1) {
        // skip forward until #endif as we are inside #if 0
    } else if (point->if_skip_depth == 1) {
        // include after the #else
        point->if_skip_depth = 0;
    } else {
        // skip until #endif
        point->if_skip_depth = 1;
    }

    bool at_bol = false;
    while (!at_bol && next_token(c->files.buffers[point->location.file], &point->location,
                                 token_out, &at_bol, label_value)) {
    }
    return process_token(c, p, token_out, label_value, at_bol);
}

static Result process_endif(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

static Result process_define(C* c,
                             Preprocessor* p,
                             Token* token_out,
                             cz::Allocated<cz::String>* label_value) {
    Location start = token_out->start;
    Location end = token_out->end;

    IncludeInfo* point = &p->include_stack.last();
    Location backup = point->location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->location.file], &point->location, token_out, &at_bol,
                    label_value)) {
        c->report_error(point->location.file, start, end, "Must give the macro a name");
        at_bol = false;
        while (!at_bol && next_token(c->files.buffers[point->location.file], &point->location,
                                     token_out, &at_bol, label_value)) {
        }
        return process_token(c, p, token_out, label_value, at_bol);
    }

    if (at_bol) {
        c->report_error(point->location.file, backup, point->location,
                        "Must give the macro a name");
        at_bol = false;
        while (!at_bol && next_token(c->files.buffers[point->location.file], &point->location,
                                     token_out, &at_bol, label_value)) {
        }
        return process_token(c, p, token_out, label_value, at_bol);
    }

    if (token_out->type != Token::Label) {
        c->report_error(point->location.file, token_out->start, token_out->end,
                        "Must give the macro a name");
        at_bol = false;
        while (!at_bol && next_token(c->files.buffers[point->location.file], &point->location,
                                     token_out, &at_bol, label_value)) {
        }
        return process_token(c, p, token_out, label_value, at_bol);
    }

    cz::String definition_name = label_value->object.clone(c->allocator);
    CZ_DEFER(definition_name.drop(c->allocator));

    Definition definition;
    definition.is_function = false;

    at_bol = false;
    while (!at_bol && next_token(c->files.buffers[point->location.file], &point->location,
                                 token_out, &at_bol, label_value)) {
        definition.tokens.reserve(c->allocator, 1);
        definition.token_values.reserve(c->allocator, 1);
        cz::String val;
        if (token_out->type == Token::Label && token_out->type == Token::String &&
            token_out->type == Token::Integer) {
            val = label_value->object.clone(c->allocator);
        }
        definition.tokens.push(*token_out);
        definition.token_values.push(val);
    }

    p->definitions.reserve(c->allocator, 1);
    auto entry = p->definitions.find(definition_name);
    entry.set(c->allocator, definition);

    return process_token(c, p, token_out, label_value, at_bol);
}

static Result process_token(C* c,
                            Preprocessor* p,
                            Token* token_out,
                            cz::Allocated<cz::String>* label_value,
                            bool at_bol) {
top:
    Location* point = &p->include_stack.last().location;
    if (at_bol && token_out->type == Token::Hash) {
        at_bol = false;
        if (next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
            if (at_bol) {
                // #\n is ignored
                goto top;
            }

            if (token_out->type == Token::Label) {
                if (label_value->object == "include") {
                    return process_include(c, p, token_out, label_value);
                }
                if (label_value->object == "pragma") {
                    return process_pragma(c, p, token_out, label_value);
                }
                if (label_value->object == "ifdef") {
                    return process_ifdef<true>(c, p, token_out, label_value);
                }
                if (label_value->object == "ifndef") {
                    return process_ifdef<false>(c, p, token_out, label_value);
                }
                if (label_value->object == "else") {
                    return process_else(c, p, token_out, label_value);
                }
                if (label_value->object == "endif") {
                    return process_endif(c, p, token_out, label_value);
                }
                if (label_value->object == "define") {
                    return process_define(c, p, token_out, label_value);
                }
            }

            c->report_error(point->file, token_out->start, token_out->end,
                            "Unknown preprocessor attribute");

            // eat until eof
            at_bol = false;
            while (!at_bol && next_token(c->files.buffers[point->file], point, token_out, &at_bol,
                                         label_value)) {
            }
            goto top;
        }
    }

    return Result::ok();
}

Result Preprocessor::next(C* c, Token* token_out, cz::Allocated<cz::String>* label_value) {
    if (include_stack.len() == 0) {
        return Result::done();
    }

    Location* point = &include_stack.last().location;
    while (point->index == c->files.buffers[point->file].len()) {
    pop_include:
        include_stack.pop();

        if (include_stack.len() == 0) {
            return Result::done();
        }

        point = &include_stack.last().location;
    }

    bool at_bol = point->index == 0;
    if (next_token(c->files.buffers[point->file], point, token_out, &at_bol, label_value)) {
        return process_token(c, this, token_out, label_value, at_bol);
    } else {
        CZ_DEBUG_ASSERT(point->index <= c->files.buffers[point->file].len());
        if (point->index < c->files.buffers[point->file].len()) {
            Location end = *point;
            next_character(c->files.buffers[point->file], &end);
            c->report_error(point->file, *point, end, "Invalid input '",
                            c->files.buffers[point->file].get(point->index), "'");
            return {Result::ErrorInvalidInput};
        }
        goto pop_include;
    }
}

}
