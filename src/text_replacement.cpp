#include "text_replacement.hpp"

namespace red {

char next_character(const FileBuffer& file_buffer, size_t* index) {
    char buffer[3];
top:
    buffer[0] = file_buffer.get(*index);
    if (buffer[0] == '\0') {
        return buffer[0];
    }

    if (buffer[0] == '?') {
        buffer[1] = file_buffer.get(*index + 1);
        if (buffer[1] == '?') {
            buffer[2] = file_buffer.get(*index + 2);
            switch (buffer[2]) {
                case '=':
                    buffer[0] = '#';
                    break;
                case '/':
                    buffer[0] = '\\';
                    break;
                case '\'':
                    buffer[0] = '^';
                    break;
                case '(':
                    buffer[0] = '[';
                    break;
                case ')':
                    buffer[0] = ']';
                    break;
                case '!':
                    buffer[0] = '|';
                    break;
                case '<':
                    buffer[0] = '{';
                    break;
                case '>':
                    buffer[0] = '}';
                    break;
                case '-':
                    buffer[0] = '~';
                    break;
                default:
                    ++*index;
                    return '?';
            }
            *index += 2;
        }
    }

    buffer[1] = file_buffer.get(*index + 1);
    if (buffer[0] == '\\' && buffer[1] == '\n') {
        *index += 2;
        goto top;
    }

    ++*index;
    return buffer[0];
}

}
