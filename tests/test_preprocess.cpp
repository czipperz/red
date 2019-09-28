#include "test_base.hpp"

#include <cz/defer.hpp>
#include <cz/heap.hpp>
#include <cz/logger.hpp>
#include "load.hpp"
#include "preprocess.hpp"

using namespace red;
using namespace red::cpp;

static FileBuffer build_file_buffer(cz::Allocator allocator, cz::Str contents) {
    FileBuffer file_buffer;
    file_buffer.buffers_len =
        (contents.len + FileBuffer::buffer_size - 1) / FileBuffer::buffer_size;
    file_buffer.last_len = contents.len % FileBuffer::buffer_size;
    file_buffer.buffers =
        static_cast<char**>(allocator.alloc({sizeof(char*) * file_buffer.buffers_len, 1}).buffer);

    for (size_t i = 0; i + 1 < file_buffer.buffers_len; ++i) {
        char** buffer = &file_buffer.buffers[i];
        *buffer = static_cast<char*>(allocator.alloc({FileBuffer::buffer_size, 1}).buffer);
        memcpy(*buffer, contents.buffer + i * FileBuffer::buffer_size, FileBuffer::buffer_size);
    }

    if (contents.len > 0) {
        char** last_buffer = &file_buffer.buffers[file_buffer.buffers_len - 1];
        *last_buffer = static_cast<char*>(allocator.alloc({file_buffer.last_len, 1}).buffer);
        memcpy(*last_buffer,
               contents.buffer + (file_buffer.buffers_len - 1) * FileBuffer::buffer_size,
               file_buffer.last_len);
    }

    return file_buffer;
}

static void setup(C* c, Preprocessor* p, cz::Str contents) {
    c->allocator = cz::heap_allocator();
    c->temp = nullptr;
    c->logger = cz::Logger::ignore();
    c->max_log_level = cz::LogLevel::Off;
    c->program_name = "*test_program*";

    load_file_reserve(c, p);

    cz::String file_path = cz::Str("*test_file*").duplicate_null_terminate(c->allocator);
    Hash file_path_hash = StrMap<size_t>::hash(file_path);

    FileBuffer file_buffer = build_file_buffer(c->allocator, contents);
    load_file_push(c, p, file_path, file_path_hash, file_buffer);
}

#define SETUP(contents)                  \
    C c;                                 \
    Preprocessor p;                      \
    setup(&c, &p, contents);             \
                                         \
    Token token;                         \
    cz::AllocatedString label_value;     \
    label_value.allocator = c.allocator; \
                                         \
    CZ_DEFER({                           \
        p.destroy(&c);                   \
        c.destroy();                     \
        label_value.drop();              \
    })

#define EAT_NEXT() p.next(&c, &token, &label_value)

TEST_CASE("Preprocessor::next empty file") {
    SETUP("");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ignores empty #pragma") {
    SETUP("#pragma\n<");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::LessThan);
    REQUIRE(token.span.start.file == 0);
    REQUIRE(token.span.start.index == 8);
    REQUIRE(token.span.start.line == 1);
    REQUIRE(token.span.start.column == 0);
    REQUIRE(token.span.end.file == 0);
    REQUIRE(token.span.end.index == 9);
    REQUIRE(token.span.end.line == 1);
    REQUIRE(token.span.end.column == 1);

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next define is skipped with value") {
    SETUP("#define x a\n");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next define is skipped no value") {
    SETUP("#define x\na");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "a");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifdef without definition is skipped") {
    SETUP("#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifndef without definition is preserved") {
    SETUP("#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifdef with definition is preserved") {
    SETUP("#define x\n#ifdef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "a");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}

TEST_CASE("Preprocessor::next ifndef with definition is skipped") {
    SETUP("#define x\n#ifndef x\na\n#endif\nb");

    REQUIRE(EAT_NEXT().type == Result::Success);
    REQUIRE(token.type == Token::Label);
    REQUIRE(label_value == "b");

    REQUIRE(EAT_NEXT().type == Result::Done);
}
