#pragma once

#include "file_contents.hpp"
#include "hashed_str.hpp"

namespace red {

struct File {
    Hashed_Str path;
    File_Contents contents;
};

}
