#pragma once

#include "parser.h"

typedef byte(*file_write_func)(word offset, const byte* data, word length);

void scan_sizes(Node* root);
byte generate_code(Node* root, file_write_func fwf);
void gen_init();
void gen_shut();