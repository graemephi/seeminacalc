#include <xmmintrin.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE __m128 fn()
{
    return _mm_rsqrt_ss(_mm_set1_ps(1.f));
}
