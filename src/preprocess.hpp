#pragma once

#include <cz/vector.hpp>
#include "context.hpp"
#include "files.hpp"
#include "location.hpp"
#include "string_map.hpp"
#include "token.hpp"

namespace red {

struct Definition {
    cz::Vector<Token> tokens;
    cz::Vector<cz::String> token_values;
    cz::Vector<cz::Str> parameters;
    bool is_function;
    bool has_varargs;
};

struct IncludeInfo {
    Location location;
    size_t if_depth;
    size_t if_skip_depth;
};

struct Preprocessor {
    cz::Vector<bool> file_pragma_once;

    cz::Vector<IncludeInfo> include_stack;
    red::StringMap<Definition> definitions;

    void push(C* c, const char* file_name, FileBuffer file_contents);
    void destroy(C* c);

    Result next(C* c, Token* token_out, cz::mem::Allocated<cz::String>* label_value);

    Location location() const;
};

}
