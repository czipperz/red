#pragma once

#include <cz/string.hpp>

namespace red {
struct Files;
struct Hashed_Str;
struct File_Contents;
struct Result;

namespace cpp {
struct Preprocessor;
}

/// Reserve a slot for an unloaded file to be included into the compilation unit.  This is exposed
/// for testing.
void include_file_reserve(Files* files, cpp::Preprocessor* preprocessor);

/// Force an unloaded file to be included into the compilation unit.  This is exposed for testing.
void force_include_file(Files* files,
                        cpp::Preprocessor* preprocessor,
                        Hashed_Str file_path,
                        File_Contents file_contents);

/// Process the file at `file_path` being included into the compilation unit.
///
/// The `file_path` must be allocated from `files.file_buffer_array` as it will be deallocated if
/// the file is already included.  It must also be null terminated so we can load it.
Result include_file(Files* files, cpp::Preprocessor* preprocessor, cz::String file_path);

}
