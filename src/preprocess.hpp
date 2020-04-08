#pragma once

#include <cz/vector.hpp>
#include "context.hpp"
#include "definition.hpp"
#include "files.hpp"
#include "location.hpp"
#include "string_map.hpp"
#include "token.hpp"

namespace red {
namespace cpp {

struct IncludeInfo {
    Location location;
    size_t if_depth;
};

struct DefinitionInfo {
    Definition* definition;
    size_t index;
};

struct Preprocessor {
    cz::Vector<bool> file_pragma_once;
    cz::Vector<IncludeInfo> include_stack;
    red::cpp::DefinitionMap definitions;
    cz::Vector<DefinitionInfo> definition_stack;

    void destroy(C* c);

    Result next(C* c, Token* token_out, cz::AllocatedString* label_value);

    Location location() const;
};

char next_character(const FileBuffer& file_buffer, Location* location);

/**
 * at_bol is an out variable but is only set to true.  Set it to true before
 * calling if at the bof otherwise false.  This dictates whether (when not in a
 * macro) Token::Hash starts a macro.
 */
bool next_token(C* c,
                const FileBuffer& file_buffer,
                Location* location,
                Token* token_out,
                bool* at_bol,
                cz::AllocatedString* label_value);

}
}
