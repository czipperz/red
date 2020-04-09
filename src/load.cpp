#include "load.hpp"

#include <cz/heap.hpp>
#include "file.hpp"
#include "file_contents.hpp"
#include "files.hpp"
#include "hashed_str.hpp"
#include "preprocess.hpp"

namespace red {

void include_file_reserve(Files* files, cpp::Preprocessor* preprocessor) {
    files->files.reserve(cz::heap_allocator(), 1);
    files->file_path_hashes.reserve(cz::heap_allocator(), 1);
    preprocessor->file_pragma_once.reserve(cz::heap_allocator(), 1);
    preprocessor->include_stack.reserve(cz::heap_allocator(), 1);
}

static void push_file(cpp::Preprocessor* preprocessor, size_t index) {
    cpp::Include_Info info = {};
    info.location.file = index;
    preprocessor->include_stack.push(info);
}

void force_include_file(Files* files,
                        cpp::Preprocessor* preprocessor,
                        Hashed_Str file_path,
                        File_Contents file_contents) {
    push_file(preprocessor, files->files.len());

    File file;
    file.path = file_path.str;
    file.contents = file_contents;
    files->files.push(file);
    files->file_path_hashes.push(file_path.hash);
    preprocessor->file_pragma_once.push(false);
}

Result include_file(Files* files, cpp::Preprocessor* preprocessor, cz::String file_path) {
    // We don't want to reload the file if we've already loaded it.
    cz::Hash hash = Hashed_Str::hash_str(file_path);
    for (size_t index = 0; index < files->file_path_hashes.len(); ++index) {
        // Todo: Maybe use a Str_Map<size_t> here?
        if (files->file_path_hashes[index] == hash && files->files[index].path == file_path) {
            // We've already included this file.
            file_path.drop(files->file_path_buffer_array.allocator());
            if (!preprocessor->file_pragma_once[index]) {
                preprocessor->include_stack.reserve(cz::heap_allocator(), 1);
                push_file(preprocessor, index);
            }
            return Result::ok();
        }
    }

    // Load the file from disk.
    include_file_reserve(files, preprocessor);

    File_Contents file_contents;
    Result result =
        file_contents.read(file_path.buffer(), files->file_array_buffer_array.allocator());
    if (result.is_err()) {
        file_contents.drop_array(files->file_array_buffer_array.allocator());
        return result;
    }

    file_path.realloc(files->file_path_buffer_array.allocator());
    force_include_file(files, preprocessor, {file_path, hash}, file_contents);

    return Result::ok();
}

}
