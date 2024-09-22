#include <chrono>

int main()
{
#if (__cpp_lib_chrono < 201907L)
#   error("Old chrono version")
#endif
}
