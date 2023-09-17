#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#define STBI_FAILURE_USERMSG

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <stb_image.h>

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif
