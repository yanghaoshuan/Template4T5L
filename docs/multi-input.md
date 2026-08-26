# 统一拉丁系 DGUS 输入法底座

`source/multi_input.c/h` 负责输入会话、光标编辑、大小写、候选词和 DGUS
事件消费。语言差异全部由 `MultiInputLanguagePack` 描述，底座本身不包含
特定语言字符或词典。

## DGUS 接口

| VP | Words | 用途 |
| --- | ---: | --- |
| `0x0700` | 1 | 按键事件，OS 消费后清零 |
| `0x0710` | 1 | 启动按键返回值，即目标文本首 VP；OS 消费后清零 |
| `0x0711` | 1 | 本次输入最大字符数，默认 64，有效范围 1–64 |
| `0x0720` | 16 | 保留组合区 |
| `0x0730`–`0x0760` | 16 each | 四个候选词 |
| `0x0770` | 16 | Caps、容量和光标状态 |
| `0x0780` | 65 | 最多 64 个 UTF-16 字符和可视光标 |

斯洛伐克键盘使用页面 11，克罗地亚键盘使用页面 13。正文、候选和目标 VP 均使用 UTF-16BE；预览中的 `|`
只表示光标，不会写入目标文本。

每个文本输入启动键都应把变量地址设为 `0x0710`，把按键返回值设为目标文本
的首 VP。固定 64 字符时无需额外操作；需要其他长度时，必须先向 `0x0711`
写入 1–64，再触发启动键。底座在启动时采样该值，随后立即把 `0x0711`
恢复为 64，因此非默认长度需要在每次启动前写入。零或大于 64 的值按 64
处理。

确认时只会从目标首 VP 开始写入 `0x0711` 指定数量的 words，未使用部分清零，
不会按固定 64 words 覆盖较短字段之后的 VP。

控制键保持 `F0/F1/F2/F3/F4/F7/F8`，候选键为 `F101–F104`，语言扩展键
从 `F200` 开始按 `extended_characters` 的顺序查表。`F300` 在活动会话中
交换主/次语言并进入新语言的 `keyboard_page`，正文、光标和 Caps 均保持。
没有注册第二语言时会忽略该事件。ASCII QWERTY 键继续
使用高字节大写、低字节小写的组合键值。

## 公共接口

```c
uint8_t MultiInputInit(MultiInputLanguagePack code *language);
uint8_t MultiInputSetLanguage(MultiInputLanguagePack code *language);
uint8_t MultiInputSetSecondaryLanguage(MultiInputLanguagePack code *language);
void MultiInputTask(void);
```

`MultiInputInit()` 校验默认语言包并清空输入法 VP。`MultiInputSetLanguage()`
和 `MultiInputSetSecondaryLanguage()` 仅允许在没有活动输入会话时调用。
运行时的 `F300` 会交换两个语言包，所以当前选择会延续到本次上电期间的下一次
输入会话；重启后仍由 `MultiInputInit()` 指定的斯洛伐克语开始。

语言包包含预组合拉丁字符的大小写表、扩展键字符表、键盘页号以及可选词典。词典单词
由 U+0000 分隔，offset 表按候选优先级指向词首；将
`dictionary_word_count` 设为零即可得到没有联想功能的基础编辑器。

## 斯洛伐克语言包

`modules/slovak_dictionary.c/h` 导出 `SlovakLanguagePack`。它包含 17 个
预组合扩展字母和 256 个按词频排序的词。输入两个连续字母后开始匹配，最多
显示四个候选；匹配不区分大小写但区分变音符号。

候选会继承用户前缀的小写、首字母大写或全大写形式。选择候选时替换光标所在
完整单词，仅当该单词位于正文末尾且仍有容量时自动补一个空格。

词典来源及可重现排序结果保存在 `slovak-ime-dictionary.tsv`。

## 克罗地亚语言包

`modules/croatian_dictionary.c/h` 导出 `CroatianLanguagePack`。它使用页面 13，
包含 `č/Č、ć/Ć、đ/Đ、š/Š、ž/Ž` 的大小写映射；`F200–F204` 按键盘物理位置
依次输入 `š、đ、ž、č、ć`。256 个候选词来自 CLARIN.SI hrWaC 2.1 高频
词形，NFC 归一化、按大小写折叠去重后生成；查询、响应 SHA-256、原始排名
和频次记录在 `croatian-ime-dictionary.tsv`。

## 新增语言

1. 在 `code` 区定义 `MultiInputCasePair`、扩展字符及可选词典数据。
2. 组装并导出一个 `MultiInputLanguagePack`，同时指定 `keyboard_page`。
3. 启动时传给 `MultiInputInit()`，或在输入法空闲时传给
   `MultiInputSetLanguage()`；需要会话内切换时再用
   `MultiInputSetSecondaryLanguage()` 注册第二语言。

当前底座处理 UTF-16 BMP 内的预组合拉丁字符，不实现组合附加符或死键序列。
