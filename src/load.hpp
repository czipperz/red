#pragma once

#include <cz/string.hpp>
#include "context.hpp"
#include "preprocess.hpp"

namespace red {

void load_file_reserve(C* c, cpp::Preprocessor* p);
void load_file_push(C* c,
                    cpp::Preprocessor* p,
                    cz::String file_path,
                    Hash file_path_hash,
                    FileBuffer file_buffer);

Result load_file(C* c, cpp::Preprocessor* p, cz::String file_path);

}
