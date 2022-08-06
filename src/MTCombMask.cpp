// Comb Mask : create a mask from a combing detection
// Comb Mask (C) 2004 Manao (manao@melix.net)

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .

#include "MTCombMask.h"
#include "VCL2/instrset.h"

template <typename T, int peak, int bits>
static void CM_C(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept
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
            for (int x{ 0 }; x < width; ++x)
            {
                const int prod{ (su[x] - s[x]) * (sd[x] - s[x]) };

                if (prod < thresinf)
                    d[x] = 0;
                else if (prod > thressup)
                    d[x] = 255;
                else
                    d[x] = prod >> 8;
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
            for (int x{ 0 }; x < width; ++x)
            {
                const int prod{ ((su[x] - s[x]) * (sd[x] - s[x])) >> (bits - 8) };

                if (prod < thresinf)
                    d[x] = 0;
                else if (prod > thressup)
                    d[x] = peak;
                else
                    d[x] = prod >> 8;
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
        const float threshinf_{ thresinf / 255.0f };
        const float thressup_{ thressup / 255.0f };

        memset(d, 0, dst_pitch * sizeof(T));
        d += dst_pitch;

        for (int y{ 1 }; y < height - 1; ++y)
        {
            for (int x{ 0 }; x < width; ++x)
            {
                const float prod{ ((su[x] - s[x]) * (sd[x] - s[x])) * 255.0f };

                if (prod < threshinf_)
                    d[x] = 0.0f;
                else if (prod > thressup_)
                    d[x] = 1.0f;
                else
                    d[x] = prod / 255.0f;
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

CombMask::CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, int opt, IScriptEnvironment* env) :
    GenericVideoFilter(_child), Yth1(thY1), Yth2(thY2), Y(y), U(u), V(v)
{
    has_at_least_v8 = env->FunctionExists("propShow");

    if (vi.IsRGB() || !vi.IsPlanar())
        env->ThrowError("CombMask: clip must be in YUV planar format.");
    if (Y > 3 || Y < 1)
        env->ThrowError("CombMask: y must be between 1..3.");
    if (U > 3 || U < 1)
        env->ThrowError("CombMask: u must be between 1..3.");
    if (V > 3 || V < 1)
        env->ThrowError("CombMask: v must be between 1..3.");
    if (thY1 > 255 || thY1 < 0)
        env->ThrowError("CombMask: first luma threshold not in the range 0..255.");
    if (thY2 > 255 || thY2 < 0)
        env->ThrowError("CombMask: second luma threshold not in the range 0..255.");
    if (thY1 > thY2)
        env->ThrowError("CombMask: the first threshold should not be superior to the second one.");
    if (opt < -1 || opt > 3)
        env->ThrowError("CombMask: opt must be between -1..3.");

    const int iset{ instrset_detect() };
    if (opt == 1 && iset < 2)
        env->ThrowError("tcolormask: opt=1 requires SSE2.");
    if (opt == 2 && iset < 8)
        env->ThrowError("tcolormask: opt=2 requires AVX2.");
    if (opt == 3 && iset < 10)
        env->ThrowError("tcolormask: opt=3 requires AVX512F.");

    const int planes[3]{ y, u, v };
    const int planecount{ std::min(vi.NumComponents(), 3) };
    for (int i{ 0 }; i < planecount; ++i)
    {
        switch (planes[i])
        {
            case 3: proccesplanes[i] = 3; break;
            case 2: proccesplanes[i] = 2; break;
            default: proccesplanes[i] = 1;
        }
    }

    switch (vi.ComponentSize())
    {
        case 1:
        {
            if ((opt == -1 && iset >= 10) || opt == 3)
                CM = CM_AVX512<uint8_t, 255, 8>;
            else if ((opt == -1 && iset >= 8) || opt == 2)
                CM = CM_AVX2<uint8_t, 255, 8>;
            else if ((opt == -1 && iset >= 2) || opt == 1)
                CM = CM_SSE2<uint8_t, 255, 8>;
            else
                CM = CM_C<uint8_t, 255, 8>;

            break;
        }
        case 2:
        {
            const int scale{ static_cast<int>(((1 << vi.BitsPerComponent()) - 1) / 255.0 + 0.5) };
            Yth1 *= scale;
            Yth2 *= scale;

            switch (vi.BitsPerComponent())
            {
                case 10:
                {
                    if ((opt == -1 && iset >= 10) || opt == 3)
                        CM = CM_AVX512<uint16_t, 1023, 10>;
                    else if ((opt == -1 && iset >= 8) || opt == 2)
                        CM = CM_AVX2<uint16_t, 1023, 10>;
                    else if ((opt == -1 && iset >= 2) || opt == 1)
                        CM = CM_SSE2<uint16_t, 1023, 10>;
                    else
                        CM = CM_C<uint16_t, 1023, 10>;

                    break;
                }
                case 12:
                {
                    if ((opt == -1 && iset >= 10) || opt == 3)
                        CM = CM_AVX512<uint16_t, 4095, 12>;
                    else if ((opt == -1 && iset >= 8) || opt == 2)
                        CM = CM_AVX2<uint16_t, 4095, 12>;
                    else if ((opt == -1 && iset >= 2) || opt == 1)
                        CM = CM_SSE2<uint16_t, 4095, 12>;
                    else
                        CM = CM_C<uint16_t, 4095, 12>;

                    break;
                }
                case 14:
                {
                    if ((opt == -1 && iset >= 10) || opt == 3)
                        CM = CM_AVX512<uint16_t, 16383, 14>;
                    else if ((opt == -1 && iset >= 8) || opt == 2)
                        CM = CM_AVX2<uint16_t, 16383, 14>;
                    else if ((opt == -1 && iset >= 2) || opt == 1)
                        CM = CM_SSE2<uint16_t, 16383, 14>;
                    else
                        CM = CM_C<uint16_t, 16383, 14>;

                    break;
                }
                default:
                {
                    if ((opt == -1 && iset >= 10) || opt == 3)
                        CM = CM_AVX512<uint16_t, 65535, 16>;
                    else if ((opt == -1 && iset >= 8) || opt == 2)
                        CM = CM_AVX2<uint16_t, 65535, 16>;
                    else if ((opt == -1 && iset >= 2) || opt == 1)
                        CM = CM_SSE2<uint16_t, 65535, 16>;
                    else
                        CM = CM_C<uint16_t, 65535, 16>;

                    break;
                }
            }
            break;
        }
        default:
        {
            if ((opt == -1 && iset >= 10) || opt == 3)
                CM = CM_AVX512<float, 1, 32>;
            else if ((opt == -1 && iset >= 8) || opt == 2)
                CM = CM_AVX2<float, 1, 32>;
            else if ((opt == -1 && iset >= 2) || opt == 1)
                CM = CM_SSE2<float, 1, 32>;
            else
                CM = CM_C<float, 1, 32>;
        }
    }
}

PVideoFrame CombMask::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame	src{ child->GetFrame(n, env) };
    PVideoFrame	dst{ has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi) };

    const int planes_y[3]{ PLANAR_Y, PLANAR_U, PLANAR_V };
    const int planecount{ std::min(vi.NumComponents(), 3) };
    for (int i{ 0 }; i < planecount; ++i)
    {
        const int src_pitch{ src->GetPitch(planes_y[i]) };
        const int dst_pitch{ dst->GetPitch(planes_y[i]) };
        const int height{ src->GetHeight(planes_y[i]) };
        const int width{ src->GetRowSize(planes_y[i]) };
        const uint8_t* srcp{ src->GetReadPtr(planes_y[i]) };
        uint8_t* __restrict dstp{ dst->GetWritePtr(planes_y[i]) };

        if (proccesplanes[i] == 3)
            CM(dstp, srcp, dst_pitch, src_pitch, height, width, Yth1, Yth2);
        else if (proccesplanes[i] == 2)
            env->BitBlt(dstp, dst_pitch, srcp, src_pitch, width, height);
    }

    if (vi.NumComponents() == 4)
        env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A), src->GetReadPtr(PLANAR_A), src->GetPitch(PLANAR_A), src->GetRowSize(PLANAR_A), src->GetHeight(PLANAR_A));

    return dst;
}

AVSValue __cdecl Create_CombMask(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    enum { Clip, ThY1, ThY2, Y, U, V, Usemmx, Opt };

    return new CombMask(
        args[Clip].AsClip(),
        args[ThY1].AsInt(30),
        args[ThY2].AsInt(30),
        args[Y].AsInt(3),
        args[U].AsInt(1),
        args[V].AsInt(1),
        args[Opt].AsInt(-1),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("CombMask", "c[thY1]i[thY2]i[y]i[u]i[v]i[usemmx]b[opt]i", Create_CombMask, 0);

    return "CombMask";
}
