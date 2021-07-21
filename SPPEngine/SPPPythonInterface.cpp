// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPEngine.h"
#include "SPPPythonInterface.h"


#if defined(_DEBUG)
#define WAS_DEBUG
#undef _DEBUG
#endif

//#define PY_SSIZE_T_CLEAN
//#include <Python.h>

#if defined(WAS_DEBUG)
#define _DEBUG
#endif

namespace SPP
{
    void CallPython()
    {
        //Py_Initialize();
        //PyRun_SimpleString("from time import time,ctime\n"
        //    "print('Today is', ctime(time()))\n");
        //if (Py_FinalizeEx() < 0) {
        //    exit(120);
        //}
    }
}