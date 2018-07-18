#pragma once
#include "targetver.h"

#pragma warning(push, 3)
#	include <Windows.h>
#pragma warning(pop)

#include <cassert>

#pragma warning(push)
#pragma warning(disable: 4365 4514 4571 4820)
#	include <memory>
#	include <vector>
#pragma warning(pop)

extern "C"
{
#pragma warning(push, 2)
#	include <libavformat/avformat.h>
#	include <libswresample/swresample.h>
#pragma warning(pop)
}
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")