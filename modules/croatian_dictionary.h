#ifndef CROATIAN_DICTIONARY_H
#define CROATIAN_DICTIONARY_H

#include "multi_input.h"

#define CROATIAN_DICTIONARY_WORD_COUNT 256U
#define CROATIAN_DICTIONARY_MAX_WORD_LENGTH 15U
#define CROATIAN_CASE_PAIR_COUNT 5U
#define CROATIAN_EXTENDED_CHARACTER_COUNT 5U
#define CROATIAN_KEYBOARD_PAGE 13U

extern code uint16_t CroatianDictionaryPool[];
extern code uint16_t CroatianDictionaryOffsets[CROATIAN_DICTIONARY_WORD_COUNT];
extern code MultiInputLanguagePack CroatianLanguagePack;

#endif /* CROATIAN_DICTIONARY_H */
