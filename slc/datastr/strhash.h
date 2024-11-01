#pragma once

#include "types.h"

typedef struct str_hash_ StrHash;

// Create a text hash database
StrHash* sh_init();

// Destroy
void sh_shut(StrHash* sh);

// Given a text string, get its hash value
word sh_get(StrHash* sh, const char* text);

// Get an generated hash value (no text associated)
word sh_temp(StrHash* sh);

// Retrieve the text associated with a hash
byte sh_text(StrHash* sh, char* text, word hash);
