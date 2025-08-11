/*
 * coreJSON v3.3.0
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


/**
 * @details 从freeRTOS LTS移植而来，主要是为了适配T5L平台的JSON解析功能。
 * 
 */



/**
 * @file core_json.h
 * @brief Include this header file to use coreJSON in your application.
 */

#ifndef CORE_JSON_H_
#define CORE_JSON_H_




typedef unsigned short size_t; /**< @brief Size type for memory operations. */


/**
 * @ingroup json_enum_types
 * @brief Return codes from coreJSON library functions.
 */
typedef enum
{
    JSONPartial = 0,      /**< @brief JSON document is valid so far but incomplete. */
    JSONSuccess,          /**< @brief JSON document is valid and complete. */
    JSONIllegalDocument,  /**< @brief JSON document is invalid or malformed. */
    JSONMaxDepthExceeded, /**< @brief JSON document has nesting that exceeds JSON_MAX_DEPTH. */
    JSONNotFound,         /**< @brief Query key could not be found in the JSON document. */
    JSONNullParameter,    /**< @brief Pointer parameter passed to a function is NULL. */
    JSONBadParameter      /**< @brief Query key is empty, or any subpart is empty, or max is 0. */
} JSONStatus_t;

/**
 * @brief 解析缓冲区以判断其是否包含有效的 JSON 文档。
 *
 * @param[in] buf  要解析的缓冲区。
 * @param[in] max  缓冲区的大小。
 *
 * @note 最大嵌套深度可通过定义宏 JSON_MAX_DEPTH 指定。默认值为 32。
 *
 * @note 默认情况下，有效的 JSON 文档可以只包含一个元素（如字符串、布尔值、数字）。
 * 若要求有效文档必须为对象或数组，可定义 JSON_VALIDATE_COLLECTIONS_ONLY。
 *
 * @return 若缓冲区内容为有效 JSON，返回 #JSONSuccess；
 * 若 buf 为 NULL，返回 #JSONNullParameter；
 * 若 max 为 0，返回 #JSONBadParameter；
 * 若缓冲区内容不是有效 JSON，返回 #JSONIllegalDocument；
 * 若对象和数组嵌套超过阈值，返回 #JSONMaxDepthExceeded；
 * 若缓冲区内容可能有效但不完整，返回 #JSONPartial。
 *
 * <b>示例</b>
 * @code{c}
 *     // 本示例中使用的变量。
 *     JSONStatus_t result;
 *     char buffer[] = "{\"foo\":\"abc\",\"bar\":{\"foo\":\"xyz\"}}";
 *     size_t bufferLength = sizeof( buffer ) - 1;
 *
 *     result = JSON_Validate( buffer, bufferLength );
 *
 *     // JSON 文档有效。
 *     assert( result == JSONSuccess );
 * @endcode
 */
/* @[declare_json_validate] */
JSONStatus_t JSON_Validate( const char * buf,
                            size_t max );
/* @[declare_json_validate] */

/**
 * @brief 在 JSON 文档中查找指定的键或数组索引，并输出其值的指针 @p outValue。
 *
 * 任意值也可以是对象或数组，最多可嵌套一定深度。当查询字符串包含由分隔符连接的匹配键字符串或数组索引时，
 * 搜索可以深入嵌套的对象或数组。
 *
 * 例如，若提供的缓冲区内容为 <code>{"foo":"abc","bar":{"foo":"xyz"}}</code>，
 * 则查找 'foo' 会输出 <code>abc</code>，查找 'bar' 会输出
 * <code>{"foo":"xyz"}</code>，查找 'bar.foo' 会输出 <code>xyz</code>。
 *
 * 若缓冲区内容为 <code>[123,456,{"foo":"abc","bar":[88,99]}]</code>，
 * 则查找 '[1]' 会输出 <code>456</code>，查找 '[2].foo' 会输出 <code>abc</code>，
 * 查找 '[2].bar[0]' 会输出 <code>88</code>。
 *
 * 成功时，指针 @p outValue 指向 buf 中的某个位置。不会对该值进行空字符结尾处理。
 * 对于有效的 JSON，可以安全地在值末尾放置一个空字符，只要在再次搜索前将原字符恢复即可。
 *
 * @param[in] buf  要搜索的缓冲区。
 * @param[in] max  缓冲区大小。
 * @param[in] query  要查找的对象键或数组索引。
 * @param[in] queryLength  键的长度。
 * @param[out] outValue  用于接收找到的值的地址的指针。
 * @param[out] outValueLength  用于接收找到的值长度的指针。
 *
 * @note 最大嵌套深度可通过定义宏 JSON_MAX_DEPTH 指定。默认值为 32（sizeof(char)）。
 *
 * @note JSON_Search() 会进行验证，但在找到匹配的键及其值后即停止。
 * 若需验证整个 JSON 文档，请使用 JSON_Validate()。
 *
 * @return 若查询匹配且输出值，返回 #JSONSuccess；
 * 若任意指针参数为 NULL，返回 #JSONNullParameter；
 * 若查询为空、分隔符后的部分为空、max 为 0 或索引过大无法转换为有符号 32 位整数，返回 #JSONBadParameter；
 * 若查询无匹配项，返回 #JSONNotFound。
 *
 * <b>示例</b>
 * @code{c}
 *     // 本示例中使用的变量。
 *     JSONStatus_t result;
 *     char buffer[] = "{\"foo\":\"abc\",\"bar\":{\"foo\":\"xyz\"}}";
 *     size_t bufferLength = sizeof( buffer ) - 1;
 *     char query[] = "bar.foo";
 *     size_t queryLength = sizeof( query ) - 1;
 *     char * value;
 *     size_t valueLength;
 *
 *     // 如果文档保证有效，则无需调用 JSON_Validate()。
 *     result = JSON_Validate( buffer, bufferLength );
 *
 *     if( result == JSONSuccess )
 *     {
 *         result = JSON_Search( buffer, bufferLength, query, queryLength,
 *                               &value, &valueLength );
 *     }
 *
 *     if( result == JSONSuccess )
 *     {
 *         // 指针 "value" 将指向 "buffer" 中的位置。
 *         char save = value[ valueLength ];
 *         // 保存该字符后，将其设置为 '\0' 以便打印。
 *         value[ valueLength ] = '\0';
 *         // 将打印 "Found: bar.foo -> xyz"。
 *         printf( "Found: %s -> %s\n", query, value );
 *         // 恢复原字符。
 *         value[ valueLength ] = save;
 *     }
 * @endcode
 *
 * @note 最大索引值约为 20 亿（2^31 - 9）。
 */
/* @[declare_json_search] */
#define JSON_Search( buf, max, query, queryLength, outValue, outValueLength ) \
    JSON_SearchT( buf, max, query, queryLength, outValue, outValueLength, NULL )
/* @[declare_json_search] */

/**
 * @brief The largest value usable as an array index in a query
 * for JSON_Search(), ~2 billion.
 */
#define MAX_INDEX_VALUE    ( 0x7FFFFFF7 )   /* 2^31 - 9 */

/**
 * @ingroup json_enum_types
 * @brief Value types from the JSON standard.
 */
typedef enum
{
    JSONInvalid = 0, /**< @brief Not a valid JSON type. */
    JSONString,      /**< @brief A quote delimited sequence of Unicode characters. */
    JSONNumber,      /**< @brief A rational number. */
    JSONTrue,        /**< @brief The literal value true. */
    JSONFalse,       /**< @brief The literal value false. */
    JSONNull,        /**< @brief The literal value null. */
    JSONObject,      /**< @brief A collection of zero or more key-value pairs. */
    JSONArray        /**< @brief A collection of zero or more values. */
} JSONTypes_t;

/**
 * @brief Same as JSON_Search(), but also outputs a type for the value found
 *
 * See @ref JSON_Search for documentation of common behavior.
 *
 * @param[in] buf  The buffer to search.
 * @param[in] max  size of the buffer.
 * @param[in] query  The object keys and array indexes to search for.
 * @param[in] queryLength  Length of the key.
 * @param[out] outValue  A pointer to receive the address of the value found.
 * @param[out] outValueLength  A pointer to receive the length of the value found.
 * @param[out] outType  An enum indicating the JSON-specific type of the value.
 */
/* @[declare_json_searcht] */
JSONStatus_t JSON_SearchT( char * buf,
                           size_t max,
                           const char * query,
                           size_t queryLength,
                           char ** outValue,
                           size_t * outValueLength,
                           JSONTypes_t * outType );
/* @[declare_json_searcht] */

/**
 * @brief Same as JSON_SearchT(), but with const qualified buf and outValue arguments.
 *
 * See @ref JSON_Search for documentation of common behavior.
 *
 * @param[in] buf  The buffer to search.
 * @param[in] max  size of the buffer.
 * @param[in] query  The object keys and array indexes to search for.
 * @param[in] queryLength  Length of the key.
 * @param[out] outValue  A pointer to receive the address of the value found.
 * @param[out] outValueLength  A pointer to receive the length of the value found.
 * @param[out] outType  An enum indicating the JSON-specific type of the value.
 */
/* @[declare_json_searchconst] */
JSONStatus_t JSON_SearchConst( const char * buf,
                               size_t max,
                               const char * query,
                               size_t queryLength,
                               const char ** outValue,
                               size_t * outValueLength,
                               JSONTypes_t * outType );
/* @[declare_json_searchconst] */

/**
 * @ingroup json_struct_types
 * @brief Structure to represent a key-value pair.
 */
typedef struct
{
    const char * key;     /**< @brief Pointer to the code point sequence for key. */
    size_t keyLength;     /**< @brief Length of the code point sequence for key. */
    const char * value;   /**< @brief Pointer to the code point sequence for value. */
    size_t valueLength;   /**< @brief Length of the code point sequence for value. */
    JSONTypes_t jsonType; /**< @brief JSON-specific type of the value. */
} JSONPair_t;

/**
 * @brief 输出集合中的下一个键值对或值。
 *
 * 此函数可用于循环输出对象中的每个键值对，或数组中的每个值。
 * 首次调用时，start 和 next 指向的整数应初始化为 0。函数会更新它们。
 * 如果存在下一个键值对或值，则输出结构体会被填充并返回 #JSONSuccess；
 * 否则结构体保持不变并返回 #JSONNotFound。
 *
 * @param[in] buf  要搜索的缓冲区。
 * @param[in] max  缓冲区大小。
 * @param[in,out] start  集合起始的索引。
 * @param[in,out] next  查找下一个值的索引。
 * @param[out] outPair  用于接收下一个键值对的指针。
 *
 * @note 此函数要求输入为有效的 JSON 文档；请先调用 JSON_Validate()。
 *
 * @note 对于对象，outPair 结构体会包含键和值。
 * 对于数组，仅包含值（即 outPair.key 为 NULL）。
 *
 * @return 若输出了值，返回 #JSONSuccess；
 * 若缓冲区不是集合，返回 #JSONIllegalDocument；
 * 若集合中无更多值，返回 #JSONNotFound。
 *
 * <b>示例</b>
 * @code{c}
 *     // 示例中用到的变量。
 *     static char * json_types[] =
 *     {
 *         "invalid",
 *         "string",
 *         "number",
 *         "true",
 *         "false",
 *         "null",
 *         "object",
 *         "array"
 *     };
 *
 *     void show( const char * json,
 *                size_t length )
 *     {
 *         size_t start = 0, next = 0;
 *         JSONPair_t pair = { 0 };
 *         JSONStatus_t result;
 *
 *         result = JSON_Validate( json, length );
 *         if( result == JSONSuccess )
 *         {
 *             result = JSON_Iterate( json, length, &start, &next, &pair );
 *         }
 *
 *         while( result == JSONSuccess )
 *         {
 *             if( pair.key != NULL )
 *             {
 *                 printf( "key: %.*s\t", ( int ) pair.keyLength, pair.key );
 *             }
 *
 *             printf( "value: (%s) %.*s\n", json_types[ pair.jsonType ],
 *                     ( int ) pair.valueLength, pair.value );
 *
 *             result = JSON_Iterate( json, length, &start, &next, &pair );
 *         }
 *     }
 * @endcode
 */
/* @[declare_json_iterate] */
JSONStatus_t JSON_Iterate( const char * buf,
                           size_t max,
                           size_t * start,
                           size_t * next,
                           JSONPair_t * outPair );
/* @[declare_json_iterate] */


#endif /* ifndef CORE_JSON_H_ */
