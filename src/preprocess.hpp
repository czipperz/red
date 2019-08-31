#pragma once

#include <cz/vector.hpp>
#include "context.hpp"
#include "files.hpp"
#include "location.hpp"
#include "string_map.hpp"
#include "token.hpp"

namespace red {

struct Preprocessor {
    cz::SmallVector<bool, 0> file_pragma_once;

    cz::SmallVector<FileLocation, 0> include_stack;
    cz::StringMap<cz::SmallVector<Token, 0>> definitions;

    Result push(C* c, const char* file_name, FileBuffer file_contents);
    void destroy(C* c);

    Result next(C* c,
                FileLocation* location_out,
                Token* token_out,
                cz::mem::Allocated<cz::String>* label_value);
};

}
