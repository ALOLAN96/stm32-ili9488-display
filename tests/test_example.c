/** @file test_example.c @brief 示例实现前的链接/运行验收入口。 */
#include "test_support.h"
int host_demo(void);
/** @brief 示例必须检查完整生命周期的每个返回值。 */
TEST(test_host_example) { CHECK(host_demo() == 0); return 0; }
