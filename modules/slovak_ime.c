/**
 * @file slovak_ime.c
 * @brief 斯洛伐克语单行输入法、编辑器与四候选词联想。
 *
 * DGUS 通过 0x0710 发起输入会话，并把目标文本 VP 作为参数传入；键盘页
 * 随后把所有触摸事件写到 0x0700。本模块以 20 ms 周期消费并清零事件，
 * 在 xdata 中维护不含光标标记的 UTF-16 正文，确认时才写回目标 VP。
 *
 * DGUS VP 的一个 word 使用高字节在前的 UTF-16BE 格式。为避免依赖 8051
 * 本机整数端序，本文件的所有 VP 读写均显式拆分或合并高、低字节。
 */
#include "slovak_ime.h"
#include "slovak_dictionary.h"

/* 原键盘保留的控制键事件。 */
#define SLOVAK_IME_KEY_ESCAPE           0x00F0U
#define SLOVAK_IME_KEY_OK               0x00F1U
#define SLOVAK_IME_KEY_BACKSPACE        0x00F2U
#define SLOVAK_IME_KEY_DELETE           0x00F3U
#define SLOVAK_IME_KEY_CAPS             0x00F4U
#define SLOVAK_IME_KEY_LEFT             0x00F7U
#define SLOVAK_IME_KEY_RIGHT            0x00F8U
#define SLOVAK_IME_KEY_ENTER_PAIR       0x0D0DU
#define SLOVAK_IME_KEY_ENTER            0x000DU

/* 候选键与 17 个扩展字母键使用独立事件区，避免与控制键值冲突。 */
#define SLOVAK_IME_CANDIDATE_KEY_BASE   0xF101U
#define SLOVAK_IME_EXTENDED_KEY_BASE    0xF200U
#define SLOVAK_IME_EXTENDED_KEY_COUNT   17U
#define SLOVAK_IME_NO_CANDIDATE         0xFFFFU

/* 候选词输出的三种大小写形式。 */
#define SLOVAK_IME_CASE_LOWER           0U
#define SLOVAK_IME_CASE_TITLE           1U
#define SLOVAK_IME_CASE_UPPER           2U

/*
 * 正文缓冲区始终保存不带可视光标的规范数据；VpBytes 是 UTF-16BE 收发暂存区；
 * CandidateOffsets 保存当前四个候选在只读词池中的起始偏移。
 */
static uint16_t xdata SlovakImeBuffer[SLOVAK_IME_MAX_LENGTH + 1U];
static uint8_t xdata SlovakImeVpBytes[(SLOVAK_IME_MAX_LENGTH + 1U) * 2U];
static uint16_t xdata SlovakImeCandidateOffsets[SLOVAK_IME_CANDIDATE_COUNT];

/* 一次输入会话的目标、来源页面、光标以及显示状态。 */
static uint16_t SlovakImeTargetVp;
static uint16_t SlovakImeSourcePage;
static uint8_t SlovakImeLength;
static uint8_t SlovakImeCursor;
static uint8_t SlovakImeActive;
static uint8_t SlovakImeCaps;
static uint8_t SlovakImeFull;
static uint8_t SlovakImeCandidateCase;

/* 扩展键依次对应 Á Ä Č Ď É Í Ĺ Ľ Ň Ó Ô Ŕ Š Ť Ú Ý Ž 的小写码点。 */
code uint16_t SlovakImeExtendedCharacters[SLOVAK_IME_EXTENDED_KEY_COUNT] = {
    0x00E1U, 0x00E4U, 0x010DU, 0x010FU, 0x00E9U, 0x00EDU,
    0x013AU, 0x013EU, 0x0148U, 0x00F3U, 0x00F4U, 0x0155U,
    0x0161U, 0x0165U, 0x00FAU, 0x00FDU, 0x017EU
};

/* 状态栏仅使用 ASCII，字库缺失时仍能显示容量与大小写状态。 */
code uint8_t SlovakImeFullText[] = "FULL ";
code uint8_t SlovakImeCapsText[] = "CAPS ";
code uint8_t SlovakImeLowerText[] = "abc ";

/** 从指定 VP 读取一个 DGUS 大端 word。 */
static uint16_t SlovakImeReadWord(uint32_t vp)
{
    uint8_t bytes[2];
    read_dgus_vp(vp, bytes, 1U);
    return ((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1];
}

/** 把一个 16 位值显式拆为高低字节后写入指定 VP。 */
static void SlovakImeWriteWord(uint32_t vp, uint16_t value)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
    write_dgus_vp(vp, bytes, 1U);
}

/** 清零共用的 VP 字节暂存区前 byte_count 个字节。 */
static void SlovakImeZeroVpBytes(uint16_t byte_count)
{
    uint16_t i;
    for(i = 0U; i < byte_count; i++)
    {
        SlovakImeVpBytes[i] = 0U;
    }
}

/** 将指定数量的 DGUS words 写零，用于清除文本或事件区域。 */
static void SlovakImeClearVp(uint32_t vp, uint16_t word_count)
{
    SlovakImeZeroVpBytes(word_count * 2U);
    write_dgus_vp(vp, SlovakImeVpBytes, word_count);
}

/** 将 ASCII 与斯洛伐克预组合大写字母映射为小写。 */
static uint16_t SlovakImeToLower(uint16_t value)
{
    if((value >= 0x0041U) && (value <= 0x005AU))
    {
        return value + 0x0020U;
    }

    switch(value)
    {
        case 0x00C1U: return 0x00E1U;
        case 0x00C4U: return 0x00E4U;
        case 0x010CU: return 0x010DU;
        case 0x010EU: return 0x010FU;
        case 0x00C9U: return 0x00E9U;
        case 0x00CDU: return 0x00EDU;
        case 0x0139U: return 0x013AU;
        case 0x013DU: return 0x013EU;
        case 0x0147U: return 0x0148U;
        case 0x00D3U: return 0x00F3U;
        case 0x00D4U: return 0x00F4U;
        case 0x0154U: return 0x0155U;
        case 0x0160U: return 0x0161U;
        case 0x0164U: return 0x0165U;
        case 0x00DAU: return 0x00FAU;
        case 0x00DDU: return 0x00FDU;
        case 0x017DU: return 0x017EU;
        default: return value;
    }
}

/** 将 ASCII 与斯洛伐克预组合小写字母映射为大写。 */
static uint16_t SlovakImeToUpper(uint16_t value)
{
    if((value >= 0x0061U) && (value <= 0x007AU))
    {
        return value - 0x0020U;
    }

    switch(value)
    {
        case 0x00E1U: return 0x00C1U;
        case 0x00E4U: return 0x00C4U;
        case 0x010DU: return 0x010CU;
        case 0x010FU: return 0x010EU;
        case 0x00E9U: return 0x00C9U;
        case 0x00EDU: return 0x00CDU;
        case 0x013AU: return 0x0139U;
        case 0x013EU: return 0x013DU;
        case 0x0148U: return 0x0147U;
        case 0x00F3U: return 0x00D3U;
        case 0x00F4U: return 0x00D4U;
        case 0x0155U: return 0x0154U;
        case 0x0161U: return 0x0160U;
        case 0x0165U: return 0x0164U;
        case 0x00FAU: return 0x00DAU;
        case 0x00FDU: return 0x00DDU;
        case 0x017EU: return 0x017DU;
        default: return value;
    }
}

/** 判断字符是否属于本模块支持的预组合大写字母。 */
static uint8_t SlovakImeIsUpper(uint16_t value)
{
    return SlovakImeToLower(value) != value;
}

/** 判断字符是否为可参与单词边界及联想匹配的斯洛伐克字母。 */
static uint8_t SlovakImeIsLetter(uint16_t value)
{
    uint16_t lower;
    lower = SlovakImeToLower(value);

    if((lower >= 0x0061U) && (lower <= 0x007AU))
    {
        return 1U;
    }

    switch(lower)
    {
        case 0x00E1U:
        case 0x00E4U:
        case 0x010DU:
        case 0x010FU:
        case 0x00E9U:
        case 0x00EDU:
        case 0x013AU:
        case 0x013EU:
        case 0x0148U:
        case 0x00F3U:
        case 0x00F4U:
        case 0x0155U:
        case 0x0161U:
        case 0x0165U:
        case 0x00FAU:
        case 0x00FDU:
        case 0x017EU:
            return 1U;
        default:
            return 0U;
    }
}

/** 按小写、首字母大写或全大写规则转换候选词中的一个字符。 */
static uint16_t SlovakImeApplyCase(uint16_t value, uint8_t style, uint8_t position)
{
    if(style == SLOVAK_IME_CASE_UPPER)
    {
        return SlovakImeToUpper(value);
    }
    if((style == SLOVAK_IME_CASE_TITLE) && (position == 0U))
    {
        return SlovakImeToUpper(value);
    }
    return SlovakImeToLower(value);
}

/**
 * 将带可视光标的预览写到 0x0780。
 *
 * 竖线只存在于显示副本中，不进入正文缓冲区，也不会在确认时写入目标 VP。
 * 正文最多 63 字符，因此 64-word 显示区恰好容纳正文、光标和终止符。
 */
static void SlovakImeWritePreview(void)
{
    uint8_t source;
    uint8_t display;
    uint16_t value;

    SlovakImeZeroVpBytes((SLOVAK_IME_MAX_LENGTH + 1U) * 2U);
    display = 0U;
    for(source = 0U; source < SlovakImeLength; source++)
    {
        if(source == SlovakImeCursor)
        {
            /* 在光标所指正文字符之前插入可见的 U+007C。 */
            SlovakImeVpBytes[(uint16_t)display * 2U] = 0U;
            SlovakImeVpBytes[(uint16_t)display * 2U + 1U] = '|';
            display++;
        }
        value = SlovakImeBuffer[source];
        SlovakImeVpBytes[(uint16_t)display * 2U] = (uint8_t)(value >> 8);
        SlovakImeVpBytes[(uint16_t)display * 2U + 1U] = (uint8_t)value;
        display++;
    }
    if(SlovakImeCursor == SlovakImeLength)
    {
        /* 光标位于末尾时，循环不会插入竖线，需要在这里补上。 */
        SlovakImeVpBytes[(uint16_t)display * 2U] = 0U;
        SlovakImeVpBytes[(uint16_t)display * 2U + 1U] = '|';
    }
    write_dgus_vp(SLOVAK_IME_BUFFER_VP, SlovakImeVpBytes,
                  SLOVAK_IME_MAX_LENGTH + 1U);
}

/** 将不含可视光标的规范正文写回本次会话的动态目标 VP。 */
static void SlovakImeWriteTarget(void)
{
    uint8_t i;
    SlovakImeZeroVpBytes((SLOVAK_IME_MAX_LENGTH + 1U) * 2U);
    for(i = 0U; i < SlovakImeLength; i++)
    {
        SlovakImeVpBytes[(uint16_t)i * 2U] = (uint8_t)(SlovakImeBuffer[i] >> 8);
        SlovakImeVpBytes[(uint16_t)i * 2U + 1U] = (uint8_t)SlovakImeBuffer[i];
    }
    write_dgus_vp(SlovakImeTargetVp, SlovakImeVpBytes,
                  SLOVAK_IME_MAX_LENGTH + 1U);
}

/** 向 16-word 状态暂存区追加一个 ASCII 字符，并返回下一写入位置。 */
static uint8_t SlovakImeAppendStatusCharacter(uint8_t position, uint8_t value)
{
    if(position < 15U)
    {
        SlovakImeVpBytes[(uint16_t)position * 2U] = 0U;
        SlovakImeVpBytes[(uint16_t)position * 2U + 1U] = value;
        position++;
    }
    return position;
}

/** 从 8051 code 区向状态暂存区追加一个零结尾 ASCII 字符串。 */
static uint8_t SlovakImeAppendStatusText(uint8_t position, uint8_t code *text)
{
    while((*text != 0U) && (position < 15U))
    {
        position = SlovakImeAppendStatusCharacter(position, *text);
        text++;
    }
    return position;
}

/** 向状态暂存区追加 0..99 的无前导零十进制表示。 */
static uint8_t SlovakImeAppendStatusNumber(uint8_t position, uint8_t value)
{
    if(value >= 10U)
    {
        position = SlovakImeAppendStatusCharacter(position, (uint8_t)('0' + value / 10U));
    }
    return SlovakImeAppendStatusCharacter(position, (uint8_t)('0' + value % 10U));
}

/** 输出“FULL/CAPS/abc 光标/长度”状态；最多写入 15 个可见字符。 */
static void SlovakImeWriteStatus(void)
{
    uint8_t position;
    SlovakImeZeroVpBytes(32U);
    position = 0U;
    if(SlovakImeFull)
    {
        position = SlovakImeAppendStatusText(position, SlovakImeFullText);
    }
    if(SlovakImeCaps)
    {
        position = SlovakImeAppendStatusText(position, SlovakImeCapsText);
    }
    else
    {
        position = SlovakImeAppendStatusText(position, SlovakImeLowerText);
    }
    position = SlovakImeAppendStatusNumber(position, SlovakImeCursor);
    position = SlovakImeAppendStatusCharacter(position, '/');
    SlovakImeAppendStatusNumber(position, SlovakImeLength);
    write_dgus_vp(SLOVAK_IME_STATUS_VP, SlovakImeVpBytes, 16U);
}

/** 计算词池中零结尾候选词的长度，并强制受 15 字符上限约束。 */
static uint8_t SlovakImeDictionaryWordLength(uint16_t offset)
{
    uint8_t length;
    length = 0U;
    while((length < SLOVAK_DICTIONARY_MAX_WORD_LENGTH) &&
          (SlovakDictionaryPool[offset + length] != 0U))
    {
        length++;
    }
    return length;
}

/** 将指定候选槽按当前前缀的大小写形式输出到相应候选 VP。 */
static void SlovakImeWriteCandidate(uint8_t slot)
{
    uint32_t vp;
    uint16_t offset;
    uint16_t value;
    uint8_t i;

    /* 四个候选 VP 的固定步长为 0x10 words。 */
    vp = (uint32_t)SLOVAK_IME_CANDIDATE1_VP + ((uint32_t)slot * 0x10UL);
    offset = SlovakImeCandidateOffsets[slot];
    SlovakImeZeroVpBytes(32U);

    if(offset != SLOVAK_IME_NO_CANDIDATE)
    {
        for(i = 0U; i < SLOVAK_DICTIONARY_MAX_WORD_LENGTH; i++)
        {
            value = SlovakDictionaryPool[offset + i];
            if(value == 0U)
            {
                break;
            }
            value = SlovakImeApplyCase(value, SlovakImeCandidateCase, i);
            SlovakImeVpBytes[(uint16_t)i * 2U] = (uint8_t)(value >> 8);
            SlovakImeVpBytes[(uint16_t)i * 2U + 1U] = (uint8_t)value;
        }
    }
    write_dgus_vp(vp, SlovakImeVpBytes, 16U);
}

/** 从光标向两侧扫描字母，得到光标所在完整单词的半开区间 [start, end)。 */
static void SlovakImeFindWordBounds(uint8_t *start, uint8_t *end)
{
    *start = SlovakImeCursor;
    while((*start > 0U) && SlovakImeIsLetter(SlovakImeBuffer[*start - 1U]))
    {
        (*start)--;
    }

    *end = SlovakImeCursor;
    while((*end < SlovakImeLength) && SlovakImeIsLetter(SlovakImeBuffer[*end]))
    {
        (*end)++;
    }
}

/** 对候选和光标左侧前缀执行不区分大小写、但区分变音字母的精确匹配。 */
static uint8_t SlovakImePrefixMatches(uint16_t offset, uint8_t start, uint8_t prefix_length)
{
    uint8_t i;
    uint16_t dictionary_value;
    for(i = 0U; i < prefix_length; i++)
    {
        dictionary_value = SlovakDictionaryPool[offset + i];
        if((dictionary_value == 0U) ||
           (SlovakImeToLower(dictionary_value) !=
            SlovakImeToLower(SlovakImeBuffer[start + i])))
        {
            return 0U;
        }
    }
    return 1U;
}

/** 根据已输入前缀识别小写、首字母大写或全大写候选输出形式。 */
static uint8_t SlovakImeDeterminePrefixCase(uint8_t start, uint8_t prefix_length)
{
    uint8_t i;
    uint8_t upper_count;
    upper_count = 0U;

    for(i = 0U; i < prefix_length; i++)
    {
        if(SlovakImeIsUpper(SlovakImeBuffer[start + i]))
        {
            upper_count++;
        }
    }

    if(upper_count == prefix_length)
    {
        return SLOVAK_IME_CASE_UPPER;
    }
    if((upper_count == 1U) && SlovakImeIsUpper(SlovakImeBuffer[start]))
    {
        return SLOVAK_IME_CASE_TITLE;
    }
    return SLOVAK_IME_CASE_LOWER;
}

/**
 * 重建四个联想候选。
 *
 * 只有光标左侧至少两个连续字母时才检索；词典已按词频排序，因此线性扫描
 * 收集到的前四项天然保持频率优先级。若条件不满足，四个候选 VP 全部清空。
 */
static void SlovakImeUpdateCandidates(void)
{
    uint16_t dictionary_index;
    uint16_t offset;
    uint8_t start;
    uint8_t end;
    uint8_t prefix_length;
    uint8_t found;
    uint8_t i;

    for(i = 0U; i < SLOVAK_IME_CANDIDATE_COUNT; i++)
    {
        SlovakImeCandidateOffsets[i] = SLOVAK_IME_NO_CANDIDATE;
    }

    SlovakImeFindWordBounds(&start, &end);
    prefix_length = SlovakImeCursor - start;
    if(prefix_length >= 2U)
    {
        for(i = 0U; i < prefix_length; i++)
        {
            if(!SlovakImeIsLetter(SlovakImeBuffer[start + i]))
            {
                prefix_length = 0U;
                break;
            }
        }
    }

    if(prefix_length >= 2U)
    {
        SlovakImeCandidateCase = SlovakImeDeterminePrefixCase(start, prefix_length);
        found = 0U;
        for(dictionary_index = 0U;
            (dictionary_index < SLOVAK_DICTIONARY_WORD_COUNT) &&
            (found < SLOVAK_IME_CANDIDATE_COUNT);
            dictionary_index++)
        {
            offset = SlovakDictionaryOffsets[dictionary_index];
            if(SlovakImePrefixMatches(offset, start, prefix_length))
            {
                SlovakImeCandidateOffsets[found] = offset;
                found++;
            }
        }
    }

    for(i = 0U; i < SLOVAK_IME_CANDIDATE_COUNT; i++)
    {
        SlovakImeWriteCandidate(i);
    }
}

/** 在正文或光标状态变化后同步预览、状态栏和全部候选词。 */
static void SlovakImeRefresh(void)
{
    SlovakImeWritePreview();
    SlovakImeWriteStatus();
    SlovakImeUpdateCandidates();
}

/** 在光标处插入一个 UTF-16 字符；达到 63 字符时置 FULL 并拒绝插入。 */
static uint8_t SlovakImeInsertCharacter(uint16_t value)
{
    uint8_t i;
    if(SlovakImeLength >= SLOVAK_IME_MAX_LENGTH)
    {
        SlovakImeFull = 1U;
        return 0U;
    }

    /* 从尾部向右搬移，避免覆盖光标后的原有正文。 */
    i = SlovakImeLength;
    while(i > SlovakImeCursor)
    {
        SlovakImeBuffer[i] = SlovakImeBuffer[i - 1U];
        i--;
    }
    SlovakImeBuffer[SlovakImeCursor] = value;
    SlovakImeLength++;
    SlovakImeCursor++;
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeFull = 0U;
    return 1U;
}

/** 删除光标左侧字符；光标在开头时不执行任何操作。 */
static void SlovakImeBackspace(void)
{
    uint8_t i;
    if(SlovakImeCursor == 0U)
    {
        return;
    }
    for(i = SlovakImeCursor - 1U; i < SlovakImeLength - 1U; i++)
    {
        SlovakImeBuffer[i] = SlovakImeBuffer[i + 1U];
    }
    SlovakImeLength--;
    SlovakImeCursor--;
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeFull = 0U;
}

/** 删除光标当前位置字符；光标在正文末尾时不执行任何操作。 */
static void SlovakImeDelete(void)
{
    uint8_t i;
    if(SlovakImeCursor >= SlovakImeLength)
    {
        return;
    }
    for(i = SlovakImeCursor; i < SlovakImeLength - 1U; i++)
    {
        SlovakImeBuffer[i] = SlovakImeBuffer[i + 1U];
    }
    SlovakImeLength--;
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeFull = 0U;
}

/**
 * 用所选候选替换光标所在的完整单词，并把光标移动到候选词尾。
 * 候选变长或变短时先调整尾部正文；只有原单词位于文本末尾时才自动加空格。
 */
static void SlovakImeSelectCandidate(uint8_t slot)
{
    uint16_t offset;
    uint16_t value;
    uint8_t start;
    uint8_t end;
    uint8_t old_word_length;
    uint8_t candidate_length;
    uint8_t difference;
    uint8_t i;
    uint8_t old_length;

    offset = SlovakImeCandidateOffsets[slot];
    if(offset == SLOVAK_IME_NO_CANDIDATE)
    {
        return;
    }

    SlovakImeFindWordBounds(&start, &end);
    old_word_length = end - start;
    candidate_length = SlovakImeDictionaryWordLength(offset);
    old_length = SlovakImeLength;

    if(candidate_length > old_word_length)
    {
        /* 候选更长：先校验容量，再从后向前为差值腾出空间。 */
        difference = candidate_length - old_word_length;
        if((uint16_t)SlovakImeLength + difference > SLOVAK_IME_MAX_LENGTH)
        {
            SlovakImeFull = 1U;
            return;
        }
        i = SlovakImeLength;
        while(i > end)
        {
            SlovakImeBuffer[i + difference - 1U] = SlovakImeBuffer[i - 1U];
            i--;
        }
        SlovakImeLength += difference;
    }
    else if(candidate_length < old_word_length)
    {
        /* 候选更短：将原单词后的正文整体左移以收缩缓冲区。 */
        difference = old_word_length - candidate_length;
        for(i = end; i < SlovakImeLength; i++)
        {
            SlovakImeBuffer[i - difference] = SlovakImeBuffer[i];
        }
        SlovakImeLength -= difference;
    }

    /* 候选显示形式沿用用户前缀的大小写风格。 */
    for(i = 0U; i < candidate_length; i++)
    {
        value = SlovakDictionaryPool[offset + i];
        SlovakImeBuffer[start + i] =
            SlovakImeApplyCase(value, SlovakImeCandidateCase, i);
    }
    SlovakImeCursor = start + candidate_length;

    /* 只在词尾原本没有后续空白或标点且仍有容量时补一个空格。 */
    if((end == old_length) && (SlovakImeLength < SLOVAK_IME_MAX_LENGTH))
    {
        SlovakImeBuffer[SlovakImeCursor] = 0x0020U;
        SlovakImeCursor++;
        SlovakImeLength++;
    }
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeFull = 0U;
}

/** 结束输入会话；commit 非零时写回正文，取消时保持原目标内容不变。 */
static void SlovakImeFinish(uint8_t commit)
{
    if(commit)
    {
        SlovakImeWriteTarget();
    }

    SlovakImeActive = 0U;
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE1_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE2_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE3_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE4_VP, 16U);
    SwitchPageById(SlovakImeSourcePage);
}

/**
 * 从动态目标 VP 载入最多 63 个 UTF-16BE 字符，并进入键盘页。
 * 当前页面会在切换前保存，以便确认或取消后准确返回调用页面。
 */
static void SlovakImeLoadTarget(uint16_t target_vp)
{
    uint8_t i;
    uint16_t value;

    SlovakImeTargetVp = target_vp;
    SlovakImeSourcePage = SlovakImeReadWord(sysDGUS_PIC_NOW);
    SlovakImeZeroVpBytes((SLOVAK_IME_MAX_LENGTH + 1U) * 2U);
    read_dgus_vp(SlovakImeTargetVp, SlovakImeVpBytes,
                 SLOVAK_IME_MAX_LENGTH + 1U);

    SlovakImeLength = 0U;
    for(i = 0U; i < SLOVAK_IME_MAX_LENGTH; i++)
    {
        /* 显式合并高低字节，避免 8051 整数端序影响 Unicode。 */
        value = ((uint16_t)SlovakImeVpBytes[(uint16_t)i * 2U] << 8) |
                (uint16_t)SlovakImeVpBytes[(uint16_t)i * 2U + 1U];
        if(value == 0U)
        {
            break;
        }
        SlovakImeBuffer[SlovakImeLength] = value;
        SlovakImeLength++;
    }
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeCursor = SlovakImeLength;
    SlovakImeCaps = 0U;
    SlovakImeFull = 0U;
    SlovakImeActive = 1U;
    SlovakImeRefresh();
    SwitchPageById(SLOVAK_IME_KEYBOARD_PAGE);
}

/** 分派控制键、候选键、扩展字母键以及原 QWERTY/标点返回键。 */
static void SlovakImeHandleKey(uint16_t key)
{
    uint16_t value;
    uint8_t slot;

    if(key == SLOVAK_IME_KEY_ESCAPE)
    {
        SlovakImeFinish(0U);
        return;
    }
    if((key == SLOVAK_IME_KEY_OK) ||
       (key == SLOVAK_IME_KEY_ENTER) ||
       (key == SLOVAK_IME_KEY_ENTER_PAIR))
    {
        SlovakImeFinish(1U);
        return;
    }
    if(key == SLOVAK_IME_KEY_BACKSPACE)
    {
        SlovakImeBackspace();
        SlovakImeRefresh();
        return;
    }
    if(key == SLOVAK_IME_KEY_DELETE)
    {
        SlovakImeDelete();
        SlovakImeRefresh();
        return;
    }
    if(key == SLOVAK_IME_KEY_CAPS)
    {
        SlovakImeCaps = !SlovakImeCaps;
        SlovakImeFull = 0U;
        SlovakImeRefresh();
        return;
    }
    if(key == SLOVAK_IME_KEY_LEFT)
    {
        if(SlovakImeCursor > 0U)
        {
            SlovakImeCursor--;
        }
        SlovakImeFull = 0U;
        SlovakImeRefresh();
        return;
    }
    if(key == SLOVAK_IME_KEY_RIGHT)
    {
        if(SlovakImeCursor < SlovakImeLength)
        {
            SlovakImeCursor++;
        }
        SlovakImeFull = 0U;
        SlovakImeRefresh();
        return;
    }

    if((key >= SLOVAK_IME_CANDIDATE_KEY_BASE) &&
       (key < SLOVAK_IME_CANDIDATE_KEY_BASE + SLOVAK_IME_CANDIDATE_COUNT))
    {
        /* 0xF101..0xF104 映射到候选槽 0..3。 */
        slot = (uint8_t)(key - SLOVAK_IME_CANDIDATE_KEY_BASE);
        SlovakImeSelectCandidate(slot);
        SlovakImeRefresh();
        return;
    }

    if((key >= SLOVAK_IME_EXTENDED_KEY_BASE) &&
       (key < SLOVAK_IME_EXTENDED_KEY_BASE + SLOVAK_IME_EXTENDED_KEY_COUNT))
    {
        /* 扩展事件查表得到预组合小写字符，Caps 打开时再转为大写。 */
        value = SlovakImeExtendedCharacters[key - SLOVAK_IME_EXTENDED_KEY_BASE];
        if(SlovakImeCaps)
        {
            value = SlovakImeToUpper(value);
        }
        SlovakImeInsertCharacter(value);
        SlovakImeRefresh();
        return;
    }

    if((key & 0xFF00U) != 0U)
    {
        /*
         * 原 return_key 同时携带小写/大写两个 ASCII 字节：Caps 决定取高
         * 字节还是低字节；单字节标点事件则直接使用整个 key。
         */
        if(SlovakImeCaps)
        {
            value = (key >> 8) & 0x00FFU;
        }
        else
        {
            value = key & 0x00FFU;
        }
    }
    else
    {
        value = key;
    }

    if(value >= 0x0020U)
    {
        SlovakImeInsertCharacter(value);
        SlovakImeRefresh();
    }
}

/**
 * 初始化模块及其 DGUS VP 协议区。
 *
 * 除清空内部状态外，也会清除保留组合区 0x0720、候选、状态和预览，避免
 * 设备复位后显示 RAM 中残留上一次输入会话的数据。
 */
void SlovakImeInit(void)
{
    uint8_t i;
    SlovakImeTargetVp = 0U;
    SlovakImeSourcePage = 0U;
    SlovakImeLength = 0U;
    SlovakImeCursor = 0U;
    SlovakImeActive = 0U;
    SlovakImeCaps = 0U;
    SlovakImeFull = 0U;
    SlovakImeCandidateCase = SLOVAK_IME_CASE_LOWER;
    SlovakImeBuffer[0] = 0U;
    for(i = 0U; i < SLOVAK_IME_CANDIDATE_COUNT; i++)
    {
        SlovakImeCandidateOffsets[i] = SLOVAK_IME_NO_CANDIDATE;
    }

    /* 事件 VP 由 OS 消费后清零；初始化时先建立相同的空闲状态。 */
    SlovakImeWriteWord(SLOVAK_IME_KEY_VP, 0U);
    SlovakImeWriteWord(SLOVAK_IME_LAUNCH_VP, 0U);
    SlovakImeClearVp(0x0720U, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE1_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE2_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE3_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_CANDIDATE4_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_STATUS_VP, 16U);
    SlovakImeClearVp(SLOVAK_IME_BUFFER_VP, SLOVAK_IME_MAX_LENGTH + 1U);
}

/**
 * 输入法周期任务入口。
 *
 * 启动请求优先于按键处理；每个非零事件在分派前立即清零，保证一次触摸只
 * 消费一次。会话进行中收到新的启动请求会被消费但不会覆盖当前目标 VP。
 */
void SlovakImeTask(void)
{
    uint16_t launch_target;
    uint16_t key;

    launch_target = SlovakImeReadWord(SLOVAK_IME_LAUNCH_VP);
    if(launch_target != 0U)
    {
        /* 启动事件携带动态目标 VP，0x0000 被定义为无请求。 */
        SlovakImeWriteWord(SLOVAK_IME_LAUNCH_VP, 0U);
        if(!SlovakImeActive)
        {
            SlovakImeWriteWord(SLOVAK_IME_KEY_VP, 0U);
            SlovakImeLoadTarget(launch_target);
        }
    }

    key = SlovakImeReadWord(SLOVAK_IME_KEY_VP);
    if(key != 0U)
    {
        /* 先清事件再处理，后续页面切换不会造成旧按键重放。 */
        SlovakImeWriteWord(SLOVAK_IME_KEY_VP, 0U);
        if(SlovakImeActive)
        {
            SlovakImeHandleKey(key);
        }
    }
}
