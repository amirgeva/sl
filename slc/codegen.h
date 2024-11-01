#pragma once

#include "parser.h"

typedef Node* (*parse_node_func)();
typedef byte (*file_write_func)(word offset, const byte* data, word length);

//void scan_sizes(Node* root);
byte generate_code(parse_node_func parse_node_, file_write_func fwf);
void gen_init();
void gen_shut();
Vector* gen_get_functions();
Vector* gen_get_unknowns();
