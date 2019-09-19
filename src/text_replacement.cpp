#include "text_replacement.hpp"

namespace red {
namespace cpp {

char next_character(const FileBuffer& file_buffer, Location* location) {
    char buffer[3];
top:
    buffer[0] = file_buffer.get(location->index);
    if (buffer[0] == '\0') {
        return buffer[0];
    }

    if (buffer[0] == '?') {
        buffer[1] = file_buffer.get(location->index + 1);
        if (buffer[1] == '?') {
            buffer[2] = file_buffer.get(location->index + 2);
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
                    ++location->index;
                    return '?';
            }
            location->index += 2;
            location->column += 2;
        }
    }

    buffer[1] = file_buffer.get(location->index + 1);
    if (buffer[0] == '\\' && buffer[1] == '\n') {
        location->index += 2;
        ++location->line;
        location->column = 0;
        goto top;
    }

    ++location->index;
    if (buffer[0] == '\n') {
        ++location->line;
        location->column = 0;
    } else {
        ++location->column;
    }

    return buffer[0];
}

}
}
