#pragma once

#include "types.h"

void sh_init();
void sh_shut();
word sh_get(const char* text);
word sh_temp();
byte sh_text(char* text, word sh);