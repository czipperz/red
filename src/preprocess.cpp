#include "preprocess.hpp"

#include <cz/assert.hpp>
#include <cz/fs/directory.hpp>
#include <cz/logger.hpp>
#include "text_replacement.hpp"

namespace red {

Result Preprocessor::push(C* c, const char* file_name, FileBuffer file_buffer) {
    c->files.buffers.reserve(c->allocator, 1);
    c->files.names.reserve(c->allocator, 1);
    file_pragma_once.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    size_t file = c->files.buffers.len();
    c->files.buffers.push(file_buffer);
    c->files.names.push(file_name);
    file_pragma_once.push(false);
    include_stack.push({file, {}});

    return Result::ok();
}

void Preprocessor::destroy(C* c) {
    file_pragma_once.drop(c->allocator);

    include_stack.drop(c->allocator);
    definitions.drop(c->allocator);
}

static void advance_over_whitespace(const FileBuffer& buffer, Location* location) {
    Location point = *location;
    while (isspace(next_character(buffer, &point))) {
        *location = point;
    }
}

static Result read_include(const FileBuffer& buffer,
                           Location* location,
                           char target,
                           cz::mem::Allocated<cz::String>* output) {
    Location point = *location;
    char c = next_character(buffer, &point);
    while (c != target) {
        if (!c) {
            return {Result::ErrorEndOfFile};
        }
        output->object.reserve(output->allocator, 1);
        output->object.push(c);
        c = next_character(buffer, &point);
    }
    *location = point;
    return Result::ok();
}

static Result process_include(C* c,
                              Preprocessor* p,
                              FileLocation* location_out,
                              Token* token_out,
                              cz::mem::Allocated<cz::String>* label_value) {
    FileLocation* point = &p->include_stack.last().location;
    advance_over_whitespace(c->files.buffers[point->file], &point->location);

    Location backup = point->location;
    char ch = next_character(c->files.buffers[point->file], &point->location);
    if (ch != '<' && ch != '"') {
        point->location = backup;
        CZ_PANIC("Unimplemented #include macro");
    }

    cz::mem::Allocated<cz::String> file_name;
    // @TODO: Use a multi arena allocator here instead of fragmenting it.
    file_name.allocator = c->allocator;

    if (ch == '"') {
        cz::Str directory_name = cz::fs::directory_component(c->files.names[point->file]);
        file_name.object.reserve(file_name.allocator, directory_name.len);
        file_name.object.append(directory_name);
    }
    size_t offset = file_name.object.len();

    Location start = point->location;
    CZ_TRY(read_include(c->files.buffers[point->file], &point->location, ch == '<' ? '>' : '"',
                        &file_name));
    Location end = point->location;

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

        cz::mem::Allocated<cz::String> temp;
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

    return p->next(c, location_out, token_out, label_value);
}

static Result process_token(C* c,
                            Preprocessor* p,
                            FileLocation* location_out,
                            Token* token,
                            cz::mem::Allocated<cz::String>* label_value,
                            bool at_bol);

static Result process_pragma(C* c,
                             Preprocessor* p,
                             FileLocation* location_out,
                             Token* token_out,
                             cz::mem::Allocated<cz::String>* label_value) {
    FileLocation* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->file], &point->location, token_out, &at_bol,
                    label_value)) {
        // ignore #pragma
        return p->next(c, location_out, token_out, label_value);
    }

    if (at_bol) {
        // #pragma is ignored
        return process_token(c, p, location_out, token_out, label_value, at_bol);
    }

    if (token_out->type == Token::Label && label_value->object == "once") {
        p->file_pragma_once[point->file] = true;

        at_bol = false;
        if (!next_token(c->files.buffers[point->file], &point->location, token_out, &at_bol,
                        label_value)) {
            // #pragma once \EOF
            return p->next(c, location_out, token_out, label_value);
        }

        if (!at_bol) {
            c->report_error(point->file, token_out->start, token_out->end,
                            "#pragma once has trailing tokens");

            // eat until eof
            at_bol = false;
            while (!at_bol && next_token(c->files.buffers[point->file], &point->location, token_out,
                                         &at_bol, label_value)) {
            }
        }

        // done processing the #pragma once so get the next token
        return process_token(c, p, location_out, token_out, label_value, at_bol);
    }

    auto start = token_out->start;

    // eat until eof
    at_bol = false;
    while (!at_bol && next_token(c->files.buffers[point->file], &point->location, token_out,
                                 &at_bol, label_value)) {
    }

    c->report_error(point->file, start, token_out->end, "Unknown #pragma");

    return process_token(c, p, location_out, token_out, label_value, at_bol);
}

static Result process_if_true(C* c,
                              Preprocessor* p,
                              FileLocation* location_out,
                              Token* token_out,
                              cz::mem::Allocated<cz::String>* label_value) {
    FileLocation* point = &p->include_stack.last().location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->file], &point->location, token_out, &at_bol,
                    label_value)) {
        c->report_error(point->file, point->location, point->location,
                        "Unterminated preprocessing branch");
        return {Result::ErrorInvalidInput};
    }

    return process_token(c, p, location_out, token_out, label_value, at_bol);
}

static Result process_if_false(C* c,
                               Preprocessor* p,
                               FileLocation* location_out,
                               Token* token_out,
                               cz::mem::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

template <bool want_present>
static Result process_ifdef(C* c,
                            Preprocessor* p,
                            FileLocation* location_out,
                            Token* token_out,
                            cz::mem::Allocated<cz::String>* label_value) {
    Location ifdef_start = token_out->start;
    Location ifdef_end = token_out->end;

    PreprocessFileLocation* point = &p->include_stack.last();
    Location backup = point->location.location;
    bool at_bol = false;
    if (!next_token(c->files.buffers[point->location.file], &point->location.location, token_out,
                    &at_bol, label_value) ||
        at_bol) {
        c->report_error(point->location.file, backup, point->location.location, "No macro to test");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    if (token_out->type != Token::Label) {
        c->report_error(point->location.file, backup, point->location.location,
                        "Must test a macro name");
        // It doesn't make sense to continue after #if because we can't deduce
        // which branch to include.
        return {Result::ErrorInvalidInput};
    }

    point->if_depth++;

    if (p->definitions.find(label_value->object).is_present() == want_present) {
        return process_if_true(c, p, location_out, token_out, label_value);
    } else {
        return process_if_false(c, p, location_out, token_out, label_value);
    }
}

static Result process_else(C* c,
                           Preprocessor* p,
                           FileLocation* location_out,
                           Token* token_out,
                           cz::mem::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

static Result process_endif(C* c,
                            Preprocessor* p,
                            FileLocation* location_out,
                            Token* token_out,
                            cz::mem::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

static Result process_define(C* c,
                             Preprocessor* p,
                             FileLocation* location_out,
                             Token* token_out,
                             cz::mem::Allocated<cz::String>* label_value) {
    CZ_PANIC("Unimplemented");
}

static Result process_token(C* c,
                            Preprocessor* p,
                            FileLocation* location_out,
                            Token* token_out,
                            cz::mem::Allocated<cz::String>* label_value,
                            bool at_bol) {
top:
    FileLocation* point = &p->include_stack.last().location;
    if (at_bol && token_out->type == Token::Hash) {
        at_bol = false;
        if (next_token(c->files.buffers[point->file], &point->location, token_out, &at_bol,
                       label_value)) {
            if (at_bol) {
                // #\n is ignored
                goto top;
            }

            if (token_out->type == Token::Label) {
                if (label_value->object == "include") {
                    return process_include(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "pragma") {
                    return process_pragma(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "ifdef") {
                    return process_ifdef<true>(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "ifndef") {
                    return process_ifdef<false>(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "else") {
                    return process_else(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "endif") {
                    return process_endif(c, p, location_out, token_out, label_value);
                }
                if (label_value->object == "define") {
                    return process_define(c, p, location_out, token_out, label_value);
                }
            }

            c->report_error(point->file, token_out->start, token_out->end,
                            "Unknown preprocessor attribute");

            // eat until eof
            at_bol = false;
            while (!at_bol && next_token(c->files.buffers[point->file], &point->location, token_out,
                                         &at_bol, label_value)) {
            }
            goto top;
        }
    }

    *location_out = *point;
    return Result::ok();
}

Result Preprocessor::next(C* c,
                          FileLocation* location_out,
                          Token* token_out,
                          cz::mem::Allocated<cz::String>* label_value) {
    if (include_stack.len() == 0) {
        return Result::done();
    }

    FileLocation* point = &include_stack.last().location;
    while (point->location.index == c->files.buffers[point->file].len()) {
    pop_include:
        include_stack.pop();

        if (include_stack.len() == 0) {
            *location_out = *point;
            return Result::done();
        }

        point = &include_stack.last().location;
    }

    bool at_bol = point->location.index == 0;
    if (next_token(c->files.buffers[point->file], &point->location, token_out, &at_bol,
                   label_value)) {
        return process_token(c, this, location_out, token_out, label_value, at_bol);
    } else {
        CZ_DEBUG_ASSERT(point->location.index <= c->files.buffers[point->file].len());
        if (point->location.index < c->files.buffers[point->file].len()) {
            Location end = point->location;
            next_character(c->files.buffers[point->file], &end);
            c->report_error(point->file, point->location, end, "Invalid input '",
                            c->files.buffers[point->file].get(point->location.index), "'");
            return {Result::ErrorInvalidInput};
        }
        goto pop_include;
    }
}

}
