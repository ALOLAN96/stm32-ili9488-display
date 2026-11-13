/** @file runtime.c @brief 无 CRT 主机 DLL 的字节复制支持，仅用于测试链接。 */
#include <stddef.h>
/** @brief 编译器可能为结构复制生成 memcpy。 */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest; const unsigned char *s = src; size_t i;
    for (i = 0U; i < n; ++i) { d[i] = s[i]; } return dest;
}
/** @brief 编译器可能为零初始化生成 memset。 */
void *memset(void *dest, int value, size_t n)
{
    unsigned char *d = dest; size_t i;
    for (i = 0U; i < n; ++i) { d[i] = (unsigned char)value; } return dest;
}
