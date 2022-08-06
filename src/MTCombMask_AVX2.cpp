#include "MTCombMask.h"
#include "VCL2/vectorclass.h"

template <typename T, int peak, int bits>
void CM_AVX2(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept
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
            for (int x{ 0 }; x < width; x += 16)
            {
                const auto s_v{ Vec16s().load_16uc(s + x) };

                const auto prod{ (Vec16s().load_16uc(su + x) - s_v) * (Vec16s().load_16uc(sd + x) - s_v) };

                compress_saturated_s2u(select(prod < Vec16s(thresinf), zero_si256(),
                    select(prod > Vec16s(thressup), Vec16s(255), (prod >> 8))), zero_si256()).store(d + x);
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
            for (int x{ 0 }; x < width; x += 8)
            {
                const auto s_v{ Vec8i().load_8us(s + x) };

                const auto prod{ ((Vec8i().load_8us(su + x) - s_v) * (Vec8i().load_8us(sd + x) - s_v)) >> (bits - 8) };

                compress_saturated_s2u(select(prod < Vec8i(thresinf), zero_si256(),
                    select(prod > Vec8i(thressup), Vec8i(peak), (prod >> 8))), zero_si256()).store(d + x);
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
        const Vec8f threshinf_{ thresinf / 255.0f };
        const Vec8f thressup_{ thressup / 255.0f };

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;

        for (int y{ 1 }; y < height - 1; ++y)
        {
            for (int x{ 0 }; x < width; x += 8)
            {
                const auto s_v{ Vec8f().load(s + x) };

                const auto prod{ ((Vec8f().load(su + x) - s_v) * (Vec8f().load(sd + x) - s_v)) * Vec8f(255.0f) };

                select(prod < threshinf_, zero_8f(),
                    select(prod > thressup_, Vec8f(1.0f), prod / 255.0f)).store_nt(d + x);
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

template void CM_AVX2<uint8_t, 255, 8>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX2<uint16_t, 1023, 10>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX2<uint16_t, 4095, 12>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX2<uint16_t, 16383, 14>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX2<uint16_t, 65535, 16>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template void CM_AVX2<float, 1, 32>(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
