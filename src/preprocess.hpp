#pragma once

#include <cz/vector.hpp>
#include "context.hpp"
#include "files.hpp"
#include "location.hpp"
#include "string_map.hpp"
#include "token.hpp"

namespace red {

struct Definition {
    cz::SmallVector<cz::Str, 0> parameters;
    cz::SmallVector<Token, 0> tokens;
    bool is_function;
};

struct PreprocessFileLocation {
    FileLocation location;
    size_t if_depth;
};

struct Preprocessor {
    cz::SmallVector<bool, 0> file_pragma_once;

    cz::SmallVector<PreprocessFileLocation, 0> include_stack;
    red::StringMap<Definition> definitions;

    Result push(C* c, const char* file_name, FileBuffer file_contents);
    void destroy(C* c);

    Result next(C* c,
                FileLocation* location_out,
                Token* token_out,
                cz::mem::Allocated<cz::String>* label_value);
};

}
