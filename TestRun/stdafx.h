#pragma once
#include "targetver.h"

#include <vld.h>

#pragma warning(push, 3)
#pragma warning(disable: 4365 4514 4571 4625 4626 4710 4774 4820 5026 5027)
#	include <iostream>
#pragma warning(pop)

#pragma warning(push, 2)
#pragma warning(disable: 4365 4571 4625 4626 4711 4774 4820 5026 5027 26434 26439 26495)
#	define BOOST_PYTHON_STATIC_LIB
#	define BOOST_NUMPY_STATIC_LIB
#	include <boost/python/numpy.hpp>
#pragma warning(pop)