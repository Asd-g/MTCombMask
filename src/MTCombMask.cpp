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

static void CM_C(const uint8_t* srcp, uint8_t* derp, int src_pitch,
	int der_pitch, int row_size, int height,
	uint8_t thresinf, const uint8_t thressup)
{
	const uint8_t* s = srcp + src_pitch;
	const uint8_t* su = srcp;
	const uint8_t* sd = srcp + static_cast<int64_t>(2) * src_pitch;
	uint8_t* d = derp;
	int smod = src_pitch - row_size;
	int dmod = der_pitch - row_size;
	int x, y;

	int prod;

	for (x = 0; x < row_size - 0; x++)
	{
		*d = 0;
		d++;
	}

	d += dmod;

	for (y = 1; y < height - 1; y++)
	{
		for (x = 0; x < row_size - 0; x++)
		{
			prod = (((*(su)-(*s))) * ((*(sd)-(*s))));

			if (prod < thresinf) *d = 0;
			else if (prod > thressup) *d = 255;
			else *d = (prod >> 8);
			s++;
			su++;
			sd++;
			d++;
		}
		d += dmod;
		s += smod;
		su += smod;
		sd += smod;
	}

	for (x = 0; x < row_size - 0; x++)
	{
		*d = 0;
		d++;
	}
}

static void copy_plane(PVideoFrame& dst, PVideoFrame& src, int plane, IScriptEnvironment* env) {
	const uint8_t* srcp = src->GetReadPtr(plane);
	int src_pitch = src->GetPitch(plane);
	int height = src->GetHeight(plane);
	int row_size = src->GetRowSize(plane);
	uint8_t* destp = dst->GetWritePtr(plane);
	int dst_pitch = dst->GetPitch(plane);
	env->BitBlt(destp, dst_pitch, srcp, src_pitch, row_size, height);
}

class CombMask : public GenericVideoFilter
{
	int Yth1, Yth2;
	int Y, U, V;
	bool has_at_least_v8;

	void (*CM) (const uint8_t*, uint8_t*, int, int, int, int, const uint8_t, const uint8_t);

public:
	CombMask(PClip _child, unsigned int thY1, unsigned int thY2,
		int y, int u, int v, bool usemmx, IScriptEnvironment* env) :
		GenericVideoFilter(_child), Yth1(thY1), Yth2(thY2), Y(y), U(u), V(v)
	{
		has_at_least_v8 = true;
		try { env->CheckVersion(8); }
		catch (const AvisynthError&) { has_at_least_v8 = false; }

		if ((vi.IsRGB() && vi.BitsPerComponent() == 8) || vi.BitsPerComponent() != 8)
		{
			env->ThrowError("CombMask: clip must be Y/YUV 8-bit format.");
		}

		if (Y > 3 || Y < 1)
		{
			env->ThrowError("CombMask: y must be between 1..3");
		}

		if (U > 3 || U < 1)
		{
			env->ThrowError("CombMask: u must be between 1..3");
		}

		if (V > 3 || V < 1)
		{
			env->ThrowError("CombMask: v must be between 1..3");
		}

		if (thY1 > 255 || thY1 < 0)
		{
			env->ThrowError("CombMask: first luma threshold not in the range 0..255");
		}

		if (thY2 > 255 || thY2 < 0)
		{
			env->ThrowError("CombMask: second luma threshold not in the range 0..255");
		}

		if (thY1 > thY2)
		{
			env->ThrowError("CombMask: the first luma threshold should not be superior to the second one");
		}

		CM = CM_C;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
	{
		PVideoFrame	src = child->GetFrame(n, env);
		PVideoFrame	der;
		if (has_at_least_v8) der = env->NewVideoFrameP(vi, &src); else der = env->NewVideoFrame(vi);

		int src_pitch, der_pitch, width, height;
		const uint8_t* srcp;
		uint8_t* derp;

		int planes_y[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
		for (int i = 0; i < 3; i++)
		{
			const int plane = planes_y[i];

			src_pitch = src->GetPitch(plane);
			src_pitch = src->GetPitch(plane);
			height = src->GetHeight(plane);
			width = src->GetRowSize(plane);
			srcp = src->GetReadPtr(plane);

			der_pitch = der->GetPitch(plane);
			derp = der->GetWritePtr(plane);

			if (plane == 1)
			{
				if (Y == 3)
				{
					CM(srcp, derp, src_pitch, der_pitch, width, height, Yth1, Yth2);
				}
				else if (Y == 2)
				{
					copy_plane(der, src, plane, env);
				}
			}

			else if (plane == 2)
			{
				if (U == 3)
				{
					CM(srcp, derp, src_pitch, der_pitch, width, height, Yth1, Yth2);
				}
				else if (U == 2)
				{
					copy_plane(der, src, plane, env);
				}
			}

			else
			{
				if (V == 3)
				{
					CM(srcp, derp, src_pitch, der_pitch, width, height, Yth1, Yth2);
				}
				else if (V == 2)
				{
					copy_plane(der, src, plane, env);
				}
			}
		}

		return der;
	}

	int __stdcall SetCacheHints(int cachehints, int frame_range)
	{
		return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
	}
};

AVSValue __cdecl Create_CombMask(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new CombMask(
		args[0].AsClip(),
		args[1].AsInt(30),
		args[2].AsInt(30),
		args[3].AsInt(3),
		args[4].AsInt(1),
		args[5].AsInt(1),
		args[6].AsBool(true),
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
