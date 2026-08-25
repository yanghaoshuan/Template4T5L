#include "slovak_ime.h"
#include "slovak_dictionary.h"

#define SLOVAK_IME_KEY_ESCAPE           0x00F0U
#define SLOVAK_IME_KEY_OK               0x00F1U
#define SLOVAK_IME_KEY_BACKSPACE        0x00F2U
#define SLOVAK_IME_KEY_DELETE           0x00F3U
#define SLOVAK_IME_KEY_CAPS             0x00F4U
#define SLOVAK_IME_KEY_LEFT             0x00F7U
#define SLOVAK_IME_KEY_RIGHT            0x00F8U
#define SLOVAK_IME_KEY_ENTER_PAIR       0x0D0DU
#define SLOVAK_IME_KEY_ENTER            0x000DU

#define SLOVAK_IME_CANDIDATE_KEY_BASE   0xF101U
#define SLOVAK_IME_EXTENDED_KEY_BASE    0xF200U
#define SLOVAK_IME_EXTENDED_KEY_COUNT   17U
#define SLOVAK_IME_NO_CANDIDATE         0xFFFFU

#define SLOVAK_IME_CASE_LOWER           0U
#define SLOVAK_IME_CASE_TITLE           1U
#define SLOVAK_IME_CASE_UPPER           2U

static uint16_t xdata SlovakImeBuffer[SLOVAK_IME_MAX_LENGTH + 1U];
static uint8_t xdata SlovakImeVpBytes[(SLOVAK_IME_MAX_LENGTH + 1U) * 2U];
static uint16_t xdata SlovakImeCandidateOffsets[SLOVAK_IME_CANDIDATE_COUNT];

static uint16_t SlovakImeTargetVp;
static uint16_t SlovakImeSourcePage;
static uint8_t SlovakImeLength;
static uint8_t SlovakImeCursor;
static uint8_t SlovakImeActive;
static uint8_t SlovakImeCaps;
static uint8_t SlovakImeFull;
static uint8_t SlovakImeCandidateCase;

code uint16_t SlovakImeExtendedCharacters[SLOVAK_IME_EXTENDED_KEY_COUNT] = {
    0x00E1U, 0x00E4U, 0x010DU, 0x010FU, 0x00E9U, 0x00EDU,
    0x013AU, 0x013EU, 0x0148U, 0x00F3U, 0x00F4U, 0x0155U,
    0x0161U, 0x0165U, 0x00FAU, 0x00FDU, 0x017EU
};

code uint8_t SlovakImeFullText[] = "FULL ";
code uint8_t SlovakImeCapsText[] = "CAPS ";
code uint8_t SlovakImeLowerText[] = "abc ";

static uint16_t SlovakImeReadWord(uint32_t vp)
{
    uint8_t bytes[2];
    read_dgus_vp(vp, bytes, 1U);
    return ((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1];
}

static void SlovakImeWriteWord(uint32_t vp, uint16_t value)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
    write_dgus_vp(vp, bytes, 1U);
}

static void SlovakImeZeroVpBytes(uint16_t byte_count)
{
    uint16_t i;
    for(i = 0U; i < byte_count; i++)
    {
        SlovakImeVpBytes[i] = 0U;
    }
}

static void SlovakImeClearVp(uint32_t vp, uint16_t word_count)
{
    SlovakImeZeroVpBytes(word_count * 2U);
    write_dgus_vp(vp, SlovakImeVpBytes, word_count);
}

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

static uint8_t SlovakImeIsUpper(uint16_t value)
{
    return SlovakImeToLower(value) != value;
}

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
        SlovakImeVpBytes[(uint16_t)display * 2U] = 0U;
        SlovakImeVpBytes[(uint16_t)display * 2U + 1U] = '|';
    }
    write_dgus_vp(SLOVAK_IME_BUFFER_VP, SlovakImeVpBytes,
                  SLOVAK_IME_MAX_LENGTH + 1U);
}

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

static uint8_t SlovakImeAppendStatusText(uint8_t position, uint8_t code *text)
{
    while((*text != 0U) && (position < 15U))
    {
        position = SlovakImeAppendStatusCharacter(position, *text);
        text++;
    }
    return position;
}

static uint8_t SlovakImeAppendStatusNumber(uint8_t position, uint8_t value)
{
    if(value >= 10U)
    {
        position = SlovakImeAppendStatusCharacter(position, (uint8_t)('0' + value / 10U));
    }
    return SlovakImeAppendStatusCharacter(position, (uint8_t)('0' + value % 10U));
}

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

static void SlovakImeWriteCandidate(uint8_t slot)
{
    uint32_t vp;
    uint16_t offset;
    uint16_t value;
    uint8_t i;

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

static void SlovakImeRefresh(void)
{
    SlovakImeWritePreview();
    SlovakImeWriteStatus();
    SlovakImeUpdateCandidates();
}

static uint8_t SlovakImeInsertCharacter(uint16_t value)
{
    uint8_t i;
    if(SlovakImeLength >= SLOVAK_IME_MAX_LENGTH)
    {
        SlovakImeFull = 1U;
        return 0U;
    }

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
        difference = old_word_length - candidate_length;
        for(i = end; i < SlovakImeLength; i++)
        {
            SlovakImeBuffer[i - difference] = SlovakImeBuffer[i];
        }
        SlovakImeLength -= difference;
    }

    for(i = 0U; i < candidate_length; i++)
    {
        value = SlovakDictionaryPool[offset + i];
        SlovakImeBuffer[start + i] =
            SlovakImeApplyCase(value, SlovakImeCandidateCase, i);
    }
    SlovakImeCursor = start + candidate_length;

    if((end == old_length) && (SlovakImeLength < SLOVAK_IME_MAX_LENGTH))
    {
        SlovakImeBuffer[SlovakImeCursor] = 0x0020U;
        SlovakImeCursor++;
        SlovakImeLength++;
    }
    SlovakImeBuffer[SlovakImeLength] = 0U;
    SlovakImeFull = 0U;
}

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
        slot = (uint8_t)(key - SLOVAK_IME_CANDIDATE_KEY_BASE);
        SlovakImeSelectCandidate(slot);
        SlovakImeRefresh();
        return;
    }

    if((key >= SLOVAK_IME_EXTENDED_KEY_BASE) &&
       (key < SLOVAK_IME_EXTENDED_KEY_BASE + SLOVAK_IME_EXTENDED_KEY_COUNT))
    {
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

void SlovakImeTask(void)
{
    uint16_t launch_target;
    uint16_t key;

    launch_target = SlovakImeReadWord(SLOVAK_IME_LAUNCH_VP);
    if(launch_target != 0U)
    {
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
        SlovakImeWriteWord(SLOVAK_IME_KEY_VP, 0U);
        if(SlovakImeActive)
        {
            SlovakImeHandleKey(key);
        }
    }
}
