#include "preprocess.hpp"

#include <cz/assert.hpp>
#include <cz/fs/directory.hpp>
#include <cz/logger.hpp>
#include "text_replacement.hpp"

namespace red {

Result Preprocessor::push(C* c, const char* file_name, FileBuffer file_buffer) {
    file_buffers.reserve(c->allocator, 1);
    file_names.reserve(c->allocator, 1);
    file_pragma_once.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    include_stack.push({file_buffers.len()});
    file_buffers.push(file_buffer);
    file_names.push(file_name);
    file_pragma_once.push(false);

    return Result::ok();
}

void Preprocessor::destroy(C* c) {
    for (size_t i = 0; i < file_buffers.len(); ++i) {
        file_buffers[i].drop(c->allocator, c->allocator);
    }
    file_buffers.drop(c->allocator);
    file_names.drop(c->allocator);
    include_stack.drop(c->allocator);
}

static void advance_over_whitespace(const FileBuffer& buffer, size_t* index) {
    size_t point = *index;
    while (isspace(next_character(buffer, &point))) {
        *index = point;
    }
}

static Result read_include(const FileBuffer& buffer,
                           size_t* index,
                           char target,
                           cz::mem::Allocated<cz::String>* output) {
    size_t point = *index;
    char c = next_character(buffer, &point);
    while (c != target) {
        if (!c) {
            return {Result::ErrorEndOfFile};
        }
        output->object.reserve(output->allocator, 1);
        output->object.push(c);
        c = next_character(buffer, &point);
    }
    return Result::ok();
}

static Result process_include(C* c,
                              Preprocessor* p,
                              FileIndex* index_out,
                              Token* token_out,
                              cz::mem::Allocated<cz::String>* label_value) {
    FileIndex* point = &p->include_stack.last();
    advance_over_whitespace(p->file_buffers[point->file], &point->index);

    size_t index_backup = point->index;
    char ch = next_character(p->file_buffers[point->file], &point->index);
    if (ch != '<' && ch != '"') {
        point->index = index_backup;
        CZ_PANIC("Unimplemented #include macro");
    }

    // @LEAK: we are going to just leak this memory until we get a multi arena rolling
    cz::mem::Allocated<cz::String> file_name;
    file_name.allocator = c->allocator;

    if (ch == '"') {
        cz::Str directory_name = cz::fs::directory_component(p->file_names[point->file]);
        file_name.object.reserve(file_name.allocator, directory_name.len);
        file_name.object.append(directory_name);
    }
    size_t offset = file_name.object.len();

    CZ_TRY(read_include(p->file_buffers[point->file], &point->index, ch == '<' ? '>' : '"',
                        &file_name));
    CZ_LOG(c, Information, "Including '",
           cz::Str{file_name.object.buffer() + offset, file_name.object.len() - offset}, '\'');

    FileBuffer file_buffer;
    if (ch == '"') {
        CZ_LOG(c, Information, "Trying '", file_name.object, "'");
        file_name.object.reserve(file_name.allocator, 1);
        *file_name.object.end() = '\0';

        // these allocators are probably going to change
        auto result = file_buffer.read(file_name.object.buffer(), c->allocator, c->allocator);
        if (result.is_ok()) {
            CZ_LOG(c, Information, "Contents: \n",
                   cz::Str{file_buffer.buffers[0], file_buffer.last_len});
        } else {
            CZ_DEBUG_ASSERT(file_buffer.len() == 0);
        }
    }

    if (file_buffer.len() == 0) {
        cz::Str included_file_name = {file_name.object.buffer() + offset,
                                      file_name.object.len() - offset};

        cz::mem::Allocated<cz::String> temp;
        temp.allocator = file_name.allocator;

        for (size_t i = 0; file_buffer.len() == 0 && i < c->options.include_paths.len(); ++i) {
            // @Speed: store the include paths as Str s so we don't call strlen
            // over and over here
            cz::Str include_path(c->options.include_paths[i]);
            bool trailing_slash = include_path.ends_with("/");  // @Speed: ends_with(char)
            temp.object.reserve(temp.allocator,
                                include_path.len + !trailing_slash + included_file_name.len + 1);
            temp.object.append(include_path);
            if (!trailing_slash) {
                temp.object.push('/');
            }
            temp.object.append(included_file_name);
            *temp.object.end() = '\0';

            CZ_LOG(c, Information, "Trying '", temp.object, "'");

            // these allocators are probably going to change
            auto result = file_buffer.read(temp.object.buffer(), c->allocator, c->allocator);
            if (result.is_ok()) {
                CZ_LOG(c, Information, "Contents: \n",
                       cz::Str{file_buffer.buffers[0], file_buffer.last_len});
                file_name.object.drop(file_name.allocator);
                file_name = temp;
            } else {
                temp.object.clear();
            }
        }

        if (file_buffer.len() == 0) {
            CZ_LOG(c, Error, "Couldn't include file '", included_file_name, "'");
            return {Result::ErrorInvalidInput};
        }
    }

    p->push(c, file_name.object.buffer(), file_buffer);

    return p->next(c, index_out, token_out, label_value);
}

static Result process_token(C* c,
                            Preprocessor* p,
                            FileIndex* index_out,
                            Token* token,
                            cz::mem::Allocated<cz::String>* label_value,
                            bool at_bol);

static Result process_pragma(C* c,
                             Preprocessor* p,
                             FileIndex* index_out,
                             Token* token_out,
                             cz::mem::Allocated<cz::String>* label_value) {
    FileIndex* point = &p->include_stack.last();
    bool at_bol = false;
    if (!next_token(p->file_buffers[point->file], &point->index, token_out, &at_bol, label_value)) {
        // ignore #pragma
        return p->next(c, index_out, token_out, label_value);
    }

    if (at_bol) {
        // #pragma is ignored
        return process_token(c, p, index_out, token_out, label_value, at_bol);
    }

    if (token_out->type == Token::Label && label_value->object == "once") {
        p->file_pragma_once[point->file] = true;

        at_bol = false;
        if (!next_token(p->file_buffers[point->file], &point->index, token_out, &at_bol,
                        label_value)) {
            // #pragma once \EOF
            return p->next(c, index_out, token_out, label_value);
        }

        if (!at_bol) {
            CZ_PANIC("#pragma once has trailing tokens");  // @UserError
        }

        // done processing the #pragma once so get the next token
        return process_token(c, p, index_out, token_out, label_value, at_bol);
    }

    CZ_PANIC("#pragma unhandled");  // @UserError
}

static Result process_token(C* c,
                            Preprocessor* p,
                            FileIndex* index_out,
                            Token* token_out,
                            cz::mem::Allocated<cz::String>* label_value,
                            bool at_bol) {
top:
    FileIndex* point = &p->include_stack.last();
    if (at_bol && token_out->type == Token::Hash) {
        at_bol = false;
        if (next_token(p->file_buffers[point->file], &point->index, token_out, &at_bol,
                       label_value)) {
            if (at_bol) {
                // #\n is ignored
                goto top;
            }

            if (token_out->type == Token::Label) {
                if (label_value->object == "include") {
                    return process_include(c, p, index_out, token_out, label_value);
                }
                if (label_value->object == "pragma") {
                    return process_pragma(c, p, index_out, token_out, label_value);
                }
            }

            CZ_PANIC("user error: unknown preprocessor attribute");  // @UserError
        }
    }

    *index_out = *point;
    return Result::ok();
}

Result Preprocessor::next(C* c,
                          FileIndex* index_out,
                          Token* token_out,
                          cz::mem::Allocated<cz::String>* label_value) {
    if (include_stack.len() == 0) {
        return Result::done();
    }

    FileIndex* point = &include_stack.last();
    while (point->index == file_buffers[point->file].len()) {
    pop_include:
        include_stack.pop();

        if (include_stack.len() == 0) {
            *index_out = *point;
            return Result::done();
        }

        point = &include_stack.last();
    }

    bool at_bol = point->index == 0;
    if (next_token(file_buffers[point->file], &point->index, token_out, &at_bol, label_value)) {
        return process_token(c, this, index_out, token_out, label_value, at_bol);
    } else {
        CZ_DEBUG_ASSERT(point->index <= file_buffers[point->file].len());
        if (point->index < file_buffers[point->file].len()) {
            CZ_LOG(c, Error, "Invalid input: ", file_buffers[point->file].get(point->index), " at ",
                   point->index);
            return {Result::ErrorInvalidInput};
        }
        goto pop_include;
    }
}

}
