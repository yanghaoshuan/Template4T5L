#ifndef MULTI_INPUT_H
#define MULTI_INPUT_H

#include "sys.h"

/* 输入法任务每 20 ms 轮询一次 DGUS 启动与按键事件。 */
#define MULTI_INPUT_TASK_ID                 1U
#define MULTI_INPUT_TASK_INTERVAL           20U

/* 兼容现有屏端工程的 VP 地址和键盘页面。 */
#define MULTI_INPUT_KEY_VP                  0x0700U
#define MULTI_INPUT_LAUNCH_VP               0x0710U
#define MULTI_INPUT_LENGTH_VP               0x0711U
#define MULTI_INPUT_COMPOSITION_VP          0x0720U
#define MULTI_INPUT_CANDIDATE1_VP           0x0730U
#define MULTI_INPUT_CANDIDATE2_VP           0x0740U
#define MULTI_INPUT_CANDIDATE3_VP           0x0750U
#define MULTI_INPUT_CANDIDATE4_VP           0x0760U
#define MULTI_INPUT_STATUS_VP               0x0770U
#define MULTI_INPUT_BUFFER_VP               0x0780U
#define MULTI_INPUT_KEYBOARD_PAGE           11U

/* 正文最多 64 字符；预览区额外保留一个可视光标字位。 */
#define MULTI_INPUT_DEFAULT_LENGTH          64U
#define MULTI_INPUT_MAX_LENGTH              64U
#define MULTI_INPUT_PREVIEW_WORD_COUNT      (MULTI_INPUT_MAX_LENGTH + 1U)
#define MULTI_INPUT_CANDIDATE_COUNT         4U
#define MULTI_INPUT_CANDIDATE_MAX_LENGTH    15U

/** 一个预组合拉丁字符的大小写映射。 */
typedef struct
{
    uint16_t lower;
    uint16_t upper;
} MultiInputCasePair;

/**
 * 拉丁语言包描述。
 *
 * 所有表均存放在 8051 code 区。词典使用 U+0000 分隔字符串，offsets 按
 * 候选优先级指向词首；不需要联想时将 dictionary_word_count 设为零。
 */
typedef struct
{
    MultiInputCasePair code *case_pairs;
    uint16_t code *extended_characters;
    uint16_t code *dictionary_pool;
    uint16_t code *dictionary_offsets;
    uint16_t dictionary_word_count;
    uint8_t case_pair_count;
    uint8_t extended_character_count;
    uint8_t dictionary_max_word_length;
} MultiInputLanguagePack;

/** 初始化底座、选择默认语言，并清空输入法占用的 DGUS VP。 */
uint8_t MultiInputInit(MultiInputLanguagePack code *language);

/** 在无活动输入会话时切换语言；成功返回 1，否则返回 0。 */
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language);

/** 消费启动和按键事件；由系统调度器每 20 ms 调用一次。 */
void MultiInputTask(void);

#endif /* MULTI_INPUT_H */
