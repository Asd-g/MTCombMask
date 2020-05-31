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

	void (*CM)(PVideoFrame&, PVideoFrame&, const int, const int, const int, int, int, IScriptEnvironment*);

public:
	CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, bool usemmx, IScriptEnvironment* env);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	int __stdcall SetCacheHints(int cachehints, int frame_range)
	{
		return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
	}
};

template <typename T>
static void CM_C(PVideoFrame& der, PVideoFrame& src, const int plane, const int bits, const int component, int thresinf, int thressup, IScriptEnvironment* env) noexcept
{
	int src_pitch = src->GetPitch(plane);
	int height = src->GetHeight(plane);
	int row_size = src->GetRowSize(plane) / component;
	const T* srcp = reinterpret_cast<const T*>(src->GetReadPtr(plane));

	int der_pitch = der->GetPitch(plane);
	T* derp = reinterpret_cast<T*>(der->GetWritePtr(plane));

	src_pitch /= sizeof(T);
	der_pitch /= sizeof(T);

	thresinf <<= bits - 8;
	thressup <<= bits - 8;

	const T* s = srcp + src_pitch;
	const T* su = srcp;
	const T* sd = srcp + static_cast<int64_t>(2) * src_pitch;
	T* d = derp;
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
			prod >>= bits - 8;

			if (prod < thresinf) *d = 0;
			else if (prod > thressup) *d = (1 << bits) - 1;
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

static void copy_plane(PVideoFrame& dst, PVideoFrame& src, int plane, IScriptEnvironment* env)
{
	const uint8_t* srcp = src->GetReadPtr(plane);
	int src_pitch = src->GetPitch(plane);
	int height = src->GetHeight(plane);
	int row_size = src->GetRowSize(plane);
	uint8_t* destp = dst->GetWritePtr(plane);
	int dst_pitch = dst->GetPitch(plane);
	env->BitBlt(destp, dst_pitch, srcp, src_pitch, row_size, height);
}

CombMask::CombMask(PClip _child, int thY1, int thY2, int y, int u, int v, bool usemmx, IScriptEnvironment* env) :
	GenericVideoFilter(_child), Yth1(thY1), Yth2(thY2), Y(y), U(u), V(v)
{
	has_at_least_v8 = true;
	try { env->CheckVersion(8); }
	catch (const AvisynthError&) { has_at_least_v8 = false; }

	if ((vi.IsRGB() && vi.BitsPerComponent() != 32) || vi.BitsPerComponent() == 32)
		env->ThrowError("CombMask: clip must be Y/YUV(A) 8..16-bit format.");

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

	int planecount = min(vi.NumComponents(), 3);
	for (int i = 0; i < planecount; i++)
	{
		if (i == 0)
		{
			if (Y == 3)
				proccesplanes[i] = 3;
			else if (Y == 2)
				proccesplanes[i] = 2;
			else
				proccesplanes[i] = 1;
		}
		else if (i == 1)
		{
			if (U == 3)
				proccesplanes[i] = 3;
			else if (U == 2)
				proccesplanes[i] = 2;
			else
				proccesplanes[i] = 1;
		}
		else
		{
			if (V == 3)
				proccesplanes[i] = 3;
			else if (V == 2)
				proccesplanes[i] = 2;
			else
				proccesplanes[i] = 1;
		}
	}

	if (vi.BitsPerComponent() == 8)
		CM = CM_C<uint8_t>;
	else
		CM = CM_C<uint16_t>;
}

PVideoFrame CombMask::GetFrame(int n, IScriptEnvironment* env)
{
	PVideoFrame	src = child->GetFrame(n, env);
	PVideoFrame	der;
	if (has_at_least_v8) der = env->NewVideoFrameP(vi, &src); else der = env->NewVideoFrame(vi);
	int bits = vi.BitsPerComponent();
	int component = vi.ComponentSize();

	int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
	const int* current_planes = planes_y;
	int planecount = min(vi.NumComponents(), 3);
	for (int i = 0; i < planecount; i++)
	{
		const int plane = current_planes[i];

		if (proccesplanes[i] == 3)
			CM(der, src, plane, bits, component,  Yth1, Yth2, env);
		else if (proccesplanes[i] == 2)
			copy_plane(der, src, plane, env);
	}

	return der;
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
