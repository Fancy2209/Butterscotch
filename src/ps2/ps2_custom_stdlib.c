/**
 * Custom implementation of specific, performance-relevant stdlib functions
 * using specialized PS2EE instructions.
 */
#include <memory.h>

#define PS2INTRIN_UNSAFE
#include "ps2intrin.h"

#define VECTOR_SIZE_BYTES 16

#define IS_ALIGNED_TO(val, alignment) (((val) & ((alignment) - 1)) == 0)
#define BOTH_ALIGNED_TO(p1, p2, alignment) IS_ALIGNED_TO(((uintptr_t)(p1) | (uintptr_t)(p2)), alignment)

#define GET_LOWER_U64(vector) (*(uint64_t*)(&(vector)))

// Determine if `v < n` for each element in `n`, assuming n is in the range [0, 128].
// Taken from "Bit Twiddling Hacks".
static inline __attribute__((always_inline)) m128u8 mm_hasless_epu8(const m128u8 v, const m128u8 n) {
    const m128u8 high_bit = mm_castepu8_epu64(mm_broadcast_epu64(0x8080808080808080ull));

    // `(v - n) & ((~v) & 0x80808080)`
    return mm_and_epu8(
        mm_sub_epu8(v, n),
        mm_and_epu8(
            mm_nor_epu8(v, mm_setzero_epu8()), high_bit
        )
    );
}

static inline __attribute__((always_inline)) uint64_t mm64_hasless_epu8(const uint64_t v, const uint64_t n) {
    const uint64_t high_bit = 0x8080808080808080ull;
    return (v - n) & (~v & high_bit);
}

// Returns non-zero for each byte in `v` if `v[x] == 0`.
// Taken from "Bit Twiddling Hacks".
static inline __attribute__((always_inline)) m128u8 mm_haszero_epu8(const m128u8 v) {
    const m128u8 one = mm_castepu8_epu64(mm_broadcast_epu64(0x0101010101010101ull));
    return mm_hasless_epu8(v, one);
}

static inline __attribute__((always_inline)) uint64_t mm64_haszero_epu8(const uint64_t v) {
    const uint64_t one = 0x0101010101010101ull;
    return mm64_hasless_epu8(v, one);
}

// Vectorized `strcmp` implementation.
int strcmp (const char* s1, const char* s2) {
    if (BOTH_ALIGNED_TO(s1, s2, VECTOR_SIZE_BYTES)) {
        m128u8 s1_data = mm_load_epu8((const m128u8*)s1);
        m128u8 s2_data = mm_load_epu8((const m128u8*)s2);

        // Result is zero if all characters are identical.
        m128u8 res = mm_sub_epu8(s2_data, s1_data);
        m128u8 res_upper = mm_castepu8_epu64(mm_unpackhi_epu64(mm_castepu64_epu8(res), mm_castepu64_epu8(s1_data)));
        while ((GET_LOWER_U64(res) | GET_LOWER_U64(res_upper)) == 0) {

            // Result is non-zero if any characters are '\0'.
            // If we're at this point in the loop, we've already determined the vectors are identical,
            // so we can exit here.
            res = mm_haszero_epu8(s1_data);
            res_upper = mm_castepu8_epu64(mm_unpackhi_epu64(mm_castepu64_epu8(res), mm_castepu64_epu8(s1_data)));
            if ((GET_LOWER_U64(res) | GET_LOWER_U64(res_upper)) != 0) {
                // We know that both vectors are identical, and we've now hit a null-terminator,
                // so the strings must be identical.
                return 0;
            }

            s1 += VECTOR_SIZE_BYTES;
            s2 += VECTOR_SIZE_BYTES;
            s1_data = mm_load_epu8((const m128u8*)s1);
            s2_data = mm_load_epu8((const m128u8*)s2);
            res = mm_sub_epu8(s2_data, s1_data);
            res_upper = mm_castepu8_epu64(mm_unpackhi_epu64(mm_castepu64_epu8(res), mm_castepu64_epu8(s1_data)));
        }
    } else if (BOTH_ALIGNED_TO(s1, s2, 8)) {
        uint64_t s1_data = *((uint64_t*)s1);
        uint64_t s2_data = *((uint64_t*)s2);
        while (s1_data == s2_data) {
            if (mm64_haszero_epu8(s1_data)) {
                return 0;
            }

            s1 += 8;
            s2 += 8;
            s1_data = *((uint64_t*)s1);
            s2_data = *((uint64_t*)s2);
        }
    }

    // Scalar loop epilogue (or standard non-aligned case)
    while (*s1 == *s2) {
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }

    return *s1 - *s2;
}
