#pragma once

#include "parser.h"

void dev_print_tree_node(Node* node, int indent);
void dev_print_tree(Node* root);
void dev_init();
void dev_shut();
void dev_output(void* f);
