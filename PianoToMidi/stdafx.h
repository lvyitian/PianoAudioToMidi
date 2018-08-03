#pragma once
#include "targetver.h"

#pragma warning(push, 3)
#	include <Windows.h>
#pragma warning(pop)

#include <cassert>

#pragma warning(push, 3)
#	include <ippcore.h>
#	include <ipps.h>
#pragma warning(pop)

extern "C"
{
#pragma warning(push, 2)
#	include <libavformat/avformat.h>
#	include <libswresample/swresample.h>
#pragma warning(pop)
}
#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swresample")

#pragma warning(push, 2)
#pragma warning(disable:4365 4514 4571 4625 4626 4710 4711 4774 5026 5027 26434 26439 26451 26495)
#	include <Juce/AppConfig.h>
#	include <juce_dsp/juce_dsp.h>
#pragma warning(pop)
#pragma comment(lib, "Juce")