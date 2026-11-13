/** @file test_support.h @brief 无第三方框架的主机断言与测试入口。 */
#ifndef ILI9488_TEST_SUPPORT_H
#define ILI9488_TEST_SUPPORT_H
#include "drv_ili9488.h"
#include "mock_port.h"
#if defined(_WIN32)
#define TEST_EXPORT __declspec(dllexport)
#else
#define TEST_EXPORT __attribute__((visibility("default")))
#endif
/** 失败返回断言行号；runner 将文件名与行号一起报告。 */
#define CHECK(condition) do { if (!(condition)) { return __LINE__; } } while (0)
#define TEST(name) TEST_EXPORT int name(void)
#endif
