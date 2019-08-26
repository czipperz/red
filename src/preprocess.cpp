#include "preprocess.hpp"

#include <cz/assert.hpp>
#include <cz/fs/directory.hpp>
#include <cz/logger.hpp>
#include "text_replacement.hpp"

namespace red {

Result Preprocessor::push(C* c, const char* file_name, FileBuffer file_buffer) {
    file_buffers.reserve(c->allocator, 1);
    file_names.reserve(c->allocator, 1);
    include_stack.reserve(c->allocator, 1);

    include_stack.push({file_buffers.len()});
    file_buffers.push(file_buffer);
    file_names.push(file_name);

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

    label_value->object.clear();
    if (ch == '"') {
        cz::Str file_name = p->file_names[point->file];
        cz::Str directory_name = cz::fs::directory_component(file_name);
        label_value->object.reserve(label_value->allocator, directory_name.len);
        label_value->object.append(directory_name);
    }
    size_t offset = label_value->object.len();

    CZ_TRY(read_include(p->file_buffers[point->file], &point->index, ch == '<' ? '>' : '"',
                        label_value));
    CZ_LOG(c, Information, "Including '",
           cz::Str{label_value->object.buffer() + offset, label_value->object.len() - offset},
           "' (", label_value->object, ")");

    FileBuffer file_buffer;
    if (ch == '"') {
        label_value->object.reserve(label_value->allocator, 1);
        label_value->object[label_value->object.len()] = '\0';
        // these buffers are probably going to change
        CZ_TRY(file_buffer.read(label_value->object.buffer(), c->allocator, c->allocator));

        memmove(label_value->object.buffer(), label_value->object.buffer() + offset,
                label_value->object.len() - offset);
        label_value->object.set_len(label_value->object.len() - offset);
    }

    label_value->object.reserve(label_value->allocator, 1);
    label_value->object[label_value->object.len()] = '\0';

    // @LEAK: we are going to just leak this memory until we get a multi arena rolling
    cz::String file_name;
    file_name.reserve(c->allocator, label_value->object.len() + 1);
    file_name.append(label_value->object);
    file_name[file_name.len()] = '\0';

    p->push(c, file_name.buffer(), file_buffer);

    return p->next(c, index_out, token_out, label_value);
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
        CZ_DEBUG_ASSERT(include_stack.len() >= 1);
        if (include_stack.len() == 1) {
            include_stack.pop();
            return Result::done();
        }

        include_stack.pop();
        point = &include_stack.last();
    }

    bool at_bol = point->index == 0;
    Token token;
    if (next_token(file_buffers[point->file], &point->index, &token, &at_bol, label_value)) {
    process_token:
        if (at_bol && token.type == Token::Hash) {
            at_bol = false;
            if (next_token(file_buffers[point->file], &point->index, &token, &at_bol,
                           label_value)) {
                if (at_bol) {
                    // we read an actual token on the next line
                    goto process_token;
                }

                if (token.type == Token::Label) {
                    if (label_value->object == "include") {
                        return process_include(c, this, index_out, token_out, label_value);
                    }
                }

                CZ_PANIC("user error: unknown preprocessor attribute");
            }
        }
        *token_out = token;
        return Result::ok();
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
