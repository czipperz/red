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
    cz::Vector<cz::Str> parameters;
    bool is_function;
    bool has_varargs;
};

struct PreprocessFileLocation {
    FileLocation location;
    size_t if_depth;
};

struct Preprocessor {
    cz::Vector<bool> file_pragma_once;

    cz::Vector<PreprocessFileLocation> include_stack;
    red::StringMap<Definition> definitions;

    Result push(C* c, const char* file_name, FileBuffer file_contents);
    void destroy(C* c);

    Result next(C* c,
                FileLocation* location_out,
                Token* token_out,
                cz::mem::Allocated<cz::String>* label_value);
};

}
