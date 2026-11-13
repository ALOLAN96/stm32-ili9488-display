/** @file test_main.c @brief 无 CRT 的32位PE入口，每个 case 独立进程。 */
#ifndef ILI9488_TEST_CASE
#error "Runner must select one case"
#endif
int ILI9488_TEST_CASE(void);
/** @brief 返回断言行号，Windows 进程退出状态保留32位。 */
int entry(void) { return ILI9488_TEST_CASE(); }
