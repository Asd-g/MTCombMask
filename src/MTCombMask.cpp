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

#include <memory.h>

#include "avisynth.h"
#include "avs/minmax.h"

class CombMask : public GenericVideoFilter
{
	int Yth1, Yth2;
	int Y, U, V;
	int proccesplanes[3];
	bool has_at_least_v8;
	int peak;

	void (*CM)(uint8_t*, const uint8_t*, int, int, const int, int, int, int, const int, const int, IScriptEnvironment*);

public:
	CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	int __stdcall SetCacheHints(int cachehints, int frame_range)
	{
		return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
	}
};

template <typename T>
static void CM_C(uint8_t* dstp_, const uint8_t* srcp_, int dst_pitch, int src_pitch, const int height, int width, int thresinf, int thressup, const int peak, const int bits, IScriptEnvironment* env) noexcept
{
	dst_pitch /= sizeof(T);
	src_pitch /= sizeof(T);
	width /= sizeof(T);
	const T* su = reinterpret_cast<const T*>(srcp_);
	T* d = reinterpret_cast<T*>(dstp_);

	const T* s = su + src_pitch;
	const T* sd = su + static_cast<int64_t>(2) * src_pitch;
	int smod = src_pitch - width;
	int dmod = dst_pitch - width;

	if constexpr (std::is_same_v<T, uint8_t>)
	{
		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}

		d += dmod;

		for (int y = 1; y < height - 1; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				int prod = (((*(su)-(*s))) * ((*(sd)-(*s))));

				if (prod < thresinf)
					*d = 0;
				else if (prod > thressup)
					*d = 255;
				else
					*d = prod >> 8;

				++s;
				++su;
				++sd;
				++d;
			}

			d += dmod;
			s += smod;
			su += smod;
			sd += smod;
		}

		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}
	}
	else if constexpr (std::is_same_v<T, uint16_t>)
	{
		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}

		d += dmod;

		for (int y = 1; y < height - 1; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				int prod = (((*(su)-(*s))) * ((*(sd)-(*s)))) >> (bits - 8);

				if (prod < thresinf)
					*d = 0;
				else if (prod > thressup)
					*d = peak;
				else
					*d = prod >> 8;

				++s;
				++su;
				++sd;
				++d;
			}

			d += dmod;
			s += smod;
			su += smod;
			sd += smod;
		}

		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}
	}
	else
	{
		float threshinf_ = thresinf / 255.f;
		float thressup_ = thressup / 255.f;

		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}

		d += dmod;

		for (int y = 1; y < height - 1; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				float prod = (((*(su)-(*s))) * ((*(sd)-(*s)))) * 255.f;

				if (prod < threshinf_)
					*d = 0.f;
				else if (prod > thressup_)
					*d = 1.f;
				else
					*d = prod / 255.f;
				++s;
				++su;
				++sd;
				++d;
			}

			d += dmod;
			s += smod;
			su += smod;
			sd += smod;
		}

		for (int x = 0; x < width; ++x)
		{
			*d = 0;
			++d;
		}
	}
}

CombMask::CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, IScriptEnvironment* env) :
	GenericVideoFilter(_child), Yth1(thY1), Yth2(thY2), Y(y), U(u), V(v)
{
	has_at_least_v8 = true;
	try { env->CheckVersion(8); }
	catch (const AvisynthError&) { has_at_least_v8 = false; }

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

	const int planes[3] = { y, u, v };
	int planecount = min(vi.NumComponents(), 3);
	for (int i = 0; i < planecount; ++i)
	{
		switch (planes[i])
		{
			case 3: proccesplanes[i] = 3; break;
			case 2: proccesplanes[i] = 2; break;
			default: proccesplanes[i] = 1; break;
		}
	}

	switch (vi.ComponentSize())
	{
		case 1:	CM = CM_C<uint8_t>; break;
		case 2: CM = CM_C<uint16_t>; break;
		default: CM = CM_C<float>; break;
	}

	peak = (1 << vi.BitsPerComponent()) - 1;
	const int scale = peak / 255;

	if (vi.ComponentSize() == 2)
	{
		Yth1 *= scale;
		Yth2 *= scale;
	}
}

PVideoFrame CombMask::GetFrame(int n, IScriptEnvironment* env)
{
	PVideoFrame	src = child->GetFrame(n, env);
	PVideoFrame	dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);
	const int bits = vi.BitsPerComponent();

	int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
	int planecount = min(vi.NumComponents(), 3);
	for (int i = 0; i < planecount; ++i)
	{
		const int plane = planes_y[i];

		const int src_pitch = src->GetPitch(plane);
		const int dst_pitch = dst->GetPitch(plane);
		const int height = src->GetHeight(plane);
		const int width = src->GetRowSize(plane);
		const uint8_t* srcp = src->GetReadPtr(plane);
		uint8_t* dstp = dst->GetWritePtr(plane);

		if (proccesplanes[i] == 3)
			CM(dstp, srcp, dst_pitch, src_pitch, height, width, Yth1, Yth2, peak, bits, env);
		else if (proccesplanes[i] == 2)
			env->BitBlt(dstp, dst_pitch, srcp, src_pitch, width, height);
	}

	return dst;
}

AVSValue __cdecl Create_CombMask(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new CombMask(
		args[0].AsClip(),
		args[1].AsInt(30),
		args[2].AsInt(30),
		args[3].AsInt(3),
		args[4].AsInt(1),
		args[5].AsInt(1),
		env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
	AVS_linkage = vectors;

	env->AddFunction("CombMask", "c[thY1]i[thY2]i[y]i[u]i[v]i[usemmx]b", Create_CombMask, 0);

	return "CombMask";
}
