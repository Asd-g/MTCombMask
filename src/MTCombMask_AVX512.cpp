#include "MTCombMask.h"
#include "VCL2/vectorclass.h"

template <typename T, int peak, int bits>
void CM_AVX512(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept
{
    dst_pitch /= sizeof(T);
    src_pitch /= sizeof(T);
    width /= sizeof(T);
    const T* su{ reinterpret_cast<const T*>(srcp_) };
    T* __restrict d{ reinterpret_cast<T*>(dstp_) };

    const T* s{ su + src_pitch };
    const T* sd{ su + static_cast<int64_t>(2) * src_pitch };

    if constexpr (std::is_same_v<T, uint8_t>)
    {
        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;

        for (int y{ 1 }; y < height - 1; ++y)
        {
            for (int x{ 0 }; x < width; x += 32)
            {
                const auto s_v{ Vec32s().load_32uc(s + x) };

                const auto prod{ (Vec32s().load_32uc(su + x) - s_v) * (Vec32s().load_32uc(sd + x) - s_v) };

                compress_saturated_s2u(select(prod < Vec32s(thresinf), zero_si512(),
                    select(prod > Vec32s(thressup), Vec32s(255), (prod >> 8))), zero_si512()).store(d + x);
            }

            d += dst_pitch;
            s += src_pitch;
            su += src_pitch;
            sd += src_pitch;
        }

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;
    }
    else if constexpr (std::is_same_v<T, uint16_t>)
    {
        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;

        for (int y{ 1 }; y < height - 1; ++y)
        {
            for (int x{ 0 }; x < width; x += 16)
            {
                const auto s_v{ Vec16i().load_16us(s + x) };

                const auto prod{ ((Vec16i().load_16us(su + x) - s_v) * (Vec16i().load_16us(sd + x) - s_v)) >> (bits - 8) };

                compress_saturated_s2u(select(prod < Vec16i(thresinf), zero_si512(),
                    select(prod > Vec16i(thressup), Vec16i(peak), (prod >> 8))), zero_si512()).store(d + x);
            }

            d += dst_pitch;
            s += src_pitch;
            su += src_pitch;
            sd += src_pitch;
        }

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;
    }
    else
    {
        const Vec16f threshinf_{ thresinf / 255.0f };
        const Vec16f thressup_{ thressup / 255.0f };

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;

        for (int y{ 1 }; y < height - 1; ++y)
        {
            for (int x{ 0 }; x < width; x += 16)
            {
                const auto s_v{ Vec16f().load(s + x) };

                const auto prod{ ((Vec16f().load(su + x) - s_v) * (Vec16f().load(sd + x) - s_v)) * Vec16f(255.0f) };

                select(prod < threshinf_, zero_16f(),
                    select(prod > thressup_, Vec16f(1.0f), prod / 255.0f)).store_nt(d + x);
            }

            d += dst_pitch;
            s += src_pitch;
            su += src_pitch;
            sd += src_pitch;
        }

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;
    }
}

template void CM_AVX512<uint8_t, 255, 8>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX512<uint16_t, 1023, 10>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX512<uint16_t, 4095, 12>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX512<uint16_t, 16383, 14>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX512<uint16_t, 65535, 16>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX512<float, 1, 32>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
