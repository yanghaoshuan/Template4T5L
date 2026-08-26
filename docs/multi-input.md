# 统一拉丁系 DGUS 输入法底座

`source/multi_input.c/h` 负责输入会话、光标编辑、大小写、候选词和 DGUS
事件消费。语言差异全部由 `MultiInputLanguagePack` 描述，底座本身不包含
特定语言字符或词典。

## DGUS 接口

| VP | Words | 用途 |
| --- | ---: | --- |
| `0x0700` | 1 | 按键事件，OS 消费后清零 |
| `0x0710` | 1 | 目标文本 VP，同时作为启动请求 |
| `0x0720` | 16 | 保留组合区 |
| `0x0730`–`0x0760` | 16 each | 四个候选词 |
| `0x0770` | 16 | Caps、容量和光标状态 |
| `0x0780` | 64 | 63 个 UTF-16 字符和可视光标 |

键盘页仍为页面 11。正文、候选和目标 VP 均使用 UTF-16BE；预览中的 `|`
只表示光标，不会写入目标文本。

控制键保持 `F0/F1/F2/F3/F4/F7/F8`，候选键为 `F101–F104`，语言扩展键
从 `F200` 开始按 `extended_characters` 的顺序查表。ASCII QWERTY 键继续
使用高字节大写、低字节小写的组合键值。

## 公共接口

```c
uint8_t MultiInputInit(MultiInputLanguagePack code *language);
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language);
void MultiInputTask(void);
```

`MultiInputInit()` 校验默认语言包并清空输入法 VP。`MultiInputSetLanguage()`
仅允许在没有活动输入会话时调用，避免编辑过程中改变字符和词典规则。

语言包包含预组合拉丁字符的大小写表、扩展键字符表以及可选词典。词典单词
由 U+0000 分隔，offset 表按候选优先级指向词首；将
`dictionary_word_count` 设为零即可得到没有联想功能的基础编辑器。

## 斯洛伐克语言包

`modules/slovak_dictionary.c/h` 导出 `SlovakLanguagePack`。它包含 17 个
预组合扩展字母和 256 个按词频排序的词。输入两个连续字母后开始匹配，最多
显示四个候选；匹配不区分大小写但区分变音符号。

候选会继承用户前缀的小写、首字母大写或全大写形式。选择候选时替换光标所在
完整单词，仅当该单词位于正文末尾且仍有容量时自动补一个空格。

词典来源及可重现排序结果保存在 `slovak-ime-dictionary.tsv`。

## 新增语言

1. 在 `code` 区定义 `MultiInputCasePair`、扩展字符及可选词典数据。
2. 组装并导出一个 `MultiInputLanguagePack`。
3. 启动时传给 `MultiInputInit()`，或在输入法空闲时传给
   `MultiInputSetLanguage()`。

当前底座处理 UTF-16 BMP 内的预组合拉丁字符，不实现组合附加符或死键序列。
