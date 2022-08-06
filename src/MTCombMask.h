#pragma once

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

#include <algorithm>

#include "avisynth.h"

class CombMask : public GenericVideoFilter
{
    int Yth1, Yth2;
    int Y, U, V;
    int proccesplanes[3];
    bool has_at_least_v8;

    void (*CM)(uint8_t* __restrict, const uint8_t*, int, int, const int, int, int, int) noexcept;

public:
    CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, int opt, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
        return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
    }
};

template <typename T, int peak, int bits>
void CM_SSE2(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template <typename T, int peak, int bits>
void CM_AVX2(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
template <typename T, int peak, int bits>
void CM_AVX512(uint8_t* __restrict dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup) noexcept;
