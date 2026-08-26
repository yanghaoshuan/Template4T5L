#ifndef SLOVAK_DICTIONARY_H
#define SLOVAK_DICTIONARY_H

#include "multi_input.h"

/* 首版内置词典固定保留 256 个高频词，每个词最多 15 个 UTF-16 字符。 */
#define SLOVAK_DICTIONARY_WORD_COUNT 256U
#define SLOVAK_DICTIONARY_MAX_WORD_LENGTH 15U
#define SLOVAK_CASE_PAIR_COUNT 17U
#define SLOVAK_EXTENDED_CHARACTER_COUNT 17U

/*
 * Pool 是以 U+0000 分隔的 UTF-16 词池；Offsets 按词频顺序指向各词首字符。
 * 两个表均放入 8051 code 区，避免占用有限的 xdata RAM。
 */
extern code uint16_t SlovakDictionaryPool[];
extern code uint16_t SlovakDictionaryOffsets[SLOVAK_DICTIONARY_WORD_COUNT];

/** 可直接传给 MultiInputInit/MultiInputSetLanguage 的斯洛伐克语言包。 */
extern code MultiInputLanguagePack SlovakLanguagePack;

#endif /* SLOVAK_DICTIONARY_H */
