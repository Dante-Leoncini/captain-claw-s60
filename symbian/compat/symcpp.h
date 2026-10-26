// RVCT 2.2 honours only the last --preinclude, which Symbian reserves for rvct.h. rvct.h
// includes <symcpp.h>, and this folder comes first in the include path of the game
// libraries, so the C++03 compat header rides along here.
#include "/epoc32/include/symcpp.h"
#include "cxx03.h"
