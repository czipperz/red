#pragma once

#include "file_contents.hpp"

namespace red {

struct File {
    const char* path;
    File_Contents contents;
};

}
