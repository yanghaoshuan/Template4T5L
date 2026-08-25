#ifndef SLOVAK_DICTIONARY_H
#define SLOVAK_DICTIONARY_H

#include "sys.h"

#define SLOVAK_DICTIONARY_WORD_COUNT 256U
#define SLOVAK_DICTIONARY_MAX_WORD_LENGTH 15U

extern code uint16_t SlovakDictionaryPool[];
extern code uint16_t SlovakDictionaryOffsets[SLOVAK_DICTIONARY_WORD_COUNT];

#endif /* SLOVAK_DICTIONARY_H */
