This is a toy project just to paly around with embedded python interpreter and multi-threading.
2 main question:
1. Recheck with thread sanitizer that gil_scoped_acquire is ok to use for differnet threads python calls sync.
2. Find the possible cause of `_DeadlockError: deadlock detected by _ModuleLock` during multi-threaded python modules import.

Setup:
ubutnu 24.04
```
sudo apt install -y \
  git \
  clang lldb lld \
  build-essential \
  libssl-dev \
  zlib1g-dev \
  libbz2-dev \
  libreadline-dev \
  libsqlite3-dev \
  libffi-dev \
  liblzma-dev \
  pkg-config \
  ninja-build
```
cpython
```
git clone https://github.com/python/cpython.git
cd cpython
git checkout v3.12.11

CC=clang CXX=clang++ CFLAGS="-fsanitize=thread -g -O1 -fno-omit-frame-pointer" LDFLAGS="-fsanitize=thread" ./configure --prefix=$HOME/repos/python-tsan  --disable-optimizations --with-pydebug  --enable-shared

make -j$(nproc)
make install
```

pybind11
```
git clone https://github.com/pybind/pybind11.git
cd pybind11
git checkout v2.13.6
```

numpy
```
$HOME/repos/python-tsan/bin/python3 -m pip --version
$HOME/repos/python-tsan/bin/python3 -m pip install -U pip
$HOME/repos/python-tsan/bin/python3 -m pip install numpy
$HOME/repos/python-tsan/bin/python3 -c "import numpy as np; print(np.__version__)"
```

emdb-test
```
mkdir embd-test-build
cd embd-test-build
cmake ../embd-test/ -GNinja -DCMAKE_CXX_COMPILER=clang++ -DPython_EXECUTABLE=$HOME/repos/python-tsan/bin/python3  -DPython_ROOT_DIR=$HOME/repos/python-tsan -DPython_INCLUDE_DIR=$HOME/repos/python-tsan/include/python3.12d
ninja
```

run
```
 PYTHONHOME=$HOME/repos/python-tsan ./embed_test
```


1. move g_racy_counter out of gil_scoped_acquire to trigger TSAN error, under the gil ist's fine.
2. no issues with gil_scoped_acquire itself found
3. imports for numpy from main thread and worker threads are different. Not because of threads, but because of
   being the first call to impovt vs later calls (cached?)

   I wandered when and why gil could be released during the import


After skipping interpreter init:
```
(lldb) breakpoint set -n PyEval_SaveThread
```
we will get the stack like this
```
(lldb) bt
* thread #1, name = 'embed_test', stop reason = breakpoint 2.1
  * frame #0: 0x00007ffff7a3041a libpython3.12d.so.1.0`PyEval_SaveThread at ceval_gil.c:694:29
    frame #1: 0x00007ffff7af86a5 libpython3.12d.so.1.0`posix_getcwd(use_bytes=0) at posixmodule.c:4009:5
    frame #2: 0x00007ffff7ae6026 libpython3.12d.so.1.0`os_getcwd [inlined] os_getcwd_impl(module=<unavailable>) at posixmodule.c:4063:12
    frame #3: 0x00007ffff7ae6014 libpython3.12d.so.1.0`os_getcwd(module=<unavailable>, _unused_ignored=<unavailable>) at posixmodule.c.h:1395:12
    frame #4: 0x00007ffff7890939 libpython3.12d.so.1.0`cfunction_vectorcall_NOARGS(func=0x00007ffff58d80b0, args=<unavailable>, nargsf=<unavailable>, kwnames=<unavailable>) at methodobject.c:481:24
    frame #5: 0x00007ffff78009ac libpython3.12d.so.1.0`_PyObject_VectorcallTstate(tstate=0x00007ffff7ef6ae0, callable=0x00007ffff58d80b0, args=0x00007ffff7023420, nargsf=9223372036854775808, kwnames=0x0000000000000000) at pycore_call.h:92:11
    frame #6: 0x00007ffff7801ff3 libpython3.12d.so.1.0`PyObject_Vectorcall(callable=0x00007ffff58d80b0, args=0x00007ffff7023420, nargsf=9223372036854775808, kwnames=0x0000000000000000) at call.c:325:12
    frame #7: 0x00007ffff79d285b libpython3.12d.so.1.0`_PyEval_EvalFrameDefault(tstate=0x00007ffff7ef6ae0, frame=0x00007ffff70233b0, throwflag=0) at bytecodes.c:2715:19
    frame #8: 0x00007ffff79b5177 libpython3.12d.so.1.0`_PyEval_Vector [inlined] _PyEval_EvalFrame(tstate=0x00007ffff7ef6ae0, frame=0x00007ffff7023020, throwflag=0) at pycore_ceval.h:89:16
    frame #9: 0x00007ffff79b513d libpython3.12d.so.1.0`_PyEval_Vector(tstate=0x00007ffff7ef6ae0, func=0x00007ffff58bcc50, locals=0x0000000000000000, args=0x00007fffffffd900, argcount=2, kwnames=0x0000000000000000) at ceval.c:1685:12
    frame #10: 0x00007ffff780257f libpython3.12d.so.1.0`_PyFunction_Vectorcall(func=0x00007ffff58bcc50, stack=0x00007fffffffd900, nargsf=<unavailable>, kwnames=0x0000000000000000) at call.c:0:45
    frame #11: 0x00007ffff7804512 libpython3.12d.so.1.0`object_vacall [inlined] _PyObject_VectorcallTstate(tstate=0x00007ffff7ef6ae0, callable=0x00007ffff58bcc50, args=0x00007fffffffd900, nargsf=<unavailable>, kwnames=0x0000000000000000) at pycore_call.h:92:11
    frame #12: 0x00007ffff78044d8 libpython3.12d.so.1.0`object_vacall(tstate=0x00007ffff7ef6ae0, base=0x0000000000000000, callable=0x00007ffff58bcc50, vargs=0x00007fffffffda60) at
 call.c:850:14
    frame #13: 0x00007ffff78041de libpython3.12d.so.1.0`PyObject_CallMethodObjArgs(obj=0x0000000000000000, name=<unavailable>) at call.c:911:24
    frame #14: 0x00007ffff7a3f796 libpython3.12d.so.1.0`PyImport_ImportModuleLevelObject [inlined] import_find_and_load(tstate=0x00007ffff7ef6ae0, abs_name=0x00007ffff5689850) at import.c:2793:11
    frame #15: 0x00007ffff7a3f750 libpython3.12d.so.1.0`PyImport_ImportModuleLevelObject(name=0x00007ffff5689850, globals=<unavailable>, locals=<unavailable>, fromlist=0x00007ffff58ba6c0, level=0) at import.c:2876:15
    frame #16: 0x00007ffff79af069 libpython3.12d.so.1.0`builtin___import__ [inlined] builtin___import___impl(module=<unavailable>, name=<unavailable>, globals=<unavailable>, locals=<unavailable>, fromlist=<unavailable>, level=0) at bltinmodule.c:276:12
    frame #17: 0x00007ffff79af061 libpython3.12d.so.1.0`builtin___import__(module=<unavailable>, args=0x00007fffffffdc20, nargs=5, kwnames=0x0000000000000000) at bltinmodule.c.h:107:20
    frame #18: 0x00007ffff78906ce libpython3.12d.so.1.0`cfunction_vectorcall_FASTCALL_KEYWORDS(func=0x00007ffff58accb0, args=0x00007fffffffdc20, nargsf=5, kwnames=0x0000000000000000) at methodobject.c:438:24
    frame #19: 0x00007ffff7802d8b libpython3.12d.so.1.0`_PyObject_CallFunctionVa [inlined] _PyObject_VectorcallTstate(tstate=<unavailable>, callable=<unavailable>, args=<unavailable>, nargsf=<unavailable>, kwnames=<unavailable>) at pycore_call.h:92:11
    frame #20: 0x00007ffff7802d84 libpython3.12d.so.1.0`_PyObject_CallFunctionVa(tstate=0x00007ffff7ef6ae0, callable=0x00007ffff58accb0, format="OOOOi", va=0x00007fffffffdd50, is_size_t=0) at call.c:0
    frame #21: 0x00007ffff7802b09 libpython3.12d.so.1.0`PyObject_CallFunction(callable=0x00007ffff58accb0, format="OOOOi") at call.c:584:14
    frame #22: 0x00007ffff7a3e8bd libpython3.12d.so.1.0`PyImport_Import(module_name=0x00007ffff5689850) at import.c:3062:9
    frame #23: 0x00007ffff7a3e63e libpython3.12d.so.1.0`PyImport_ImportModule(name="numpy") at import.c:2486:14
    frame #24: 0x00005555556396b7 embed_test`main [inlined] pybind11::module_::import(name=<unavailable>) at pybind11.h:1230:25
    frame #25: 0x00005555556396ab embed_test`main(argc=<unavailable>, argv=<unavailable>) at main.cpp:51:13
    frame #26: 0x00007ffff6e2a1ca libc.so.6`___lldb_unnamed_symbol3278 + 122
    frame #27: 0x00007ffff6e2a28b libc.so.6`__libc_start_main + 139
    frame #28: 0x0000555555587bc5 embed_test`_start + 37

```

It's actually python interpreter who release the GIL during os_getcwd call. So, it's not numpy specific, it's common module loading behaviour.

For the import call in worker thread we will not trigger the breakpoint at all any get it only during numpy actuall call:

```
(lldb) bt
* thread #26, name = 'embed_test', stop reason = breakpoint 2.1
  * frame #0: 0x00007ffff7a3041a libpython3.12d.so.1.0`PyEval_SaveThread at ceval_gil.c:694:29
    frame #1: 0x00007ffff399dc65 _multiarray_umath.cpython-312-x86_64-linux-gnu.so`PyArray_ArangeObj + 2533
    frame #2: 0x00007ffff39f8bd7 _multiarray_umath.cpython-312-x86_64-linux-gnu.so`array_arange + 327
    frame #3: 0x00007ffff78906ce libpython3.12d.so.1.0`cfunction_vectorcall_FASTCALL_KEYWORDS(func=0x00007ffff54557f0, args=0x00007ffff56880b8, nargsf=1, kwnames=0x0000000000000000) at methodobject.c:438:24
    frame #4: 0x00007ffff7801ef8 libpython3.12d.so.1.0`_PyVectorcall_Call(tstate=0x000072440000a010, func=(libpython3.12d.so.1.0`cfunction_vectorcall_FASTCALL_KEYWORDS at methodobject.c:430), callable=0x00007ffff54557f0, tuple=0x00007ffff56880a0, kwargs=0x0000000000000000) at call.c:271:16
    frame #5: 0x00007ffff780217c libpython3.12d.so.1.0`_PyObject_Call(tstate=0x000072440000a010, callable=0x00007ffff54557f0, args=0x00007ffff56880a0, kwargs=0x0000000000000000) at call.c:354:16
    frame #6: 0x00007ffff780283c libpython3.12d.so.1.0`PyObject_CallObject(callable=0x00007ffff54557f0, args=0x00007ffff56880a0) at call.c:476:12
    frame #7: 0x00005555556589ac embed_test`pybind11::object pybind11::detail::object_api<pybind11::detail::accessor<pybind11::detail::accessor_policies::str_attr>>::operator()<(pybind11::return_value_policy)1, int>(int&&) const [inlined] pybind11::detail::simple_collector<(pybind11::return_value_policy)1>::call(this=0x00007fffb3102570, ptr=0x00007ffff54557f0) const at cast.h:1653:28
    frame #8: 0x000055555565899b embed_test`pybind11::object pybind11::detail::object_api<pybind11::detail::accessor<pybind11::detail::accessor_policies::str_attr>>::operator()<(pybind11::return_value_policy)1, int>(this=0x00007fffb31025f8, args=0x00007fffb31025f0) const at cast.h:1823:75
    frame #9: 0x000055555563a2f4 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] DoThreadWork(tid=<unavailable>, iters=<unavailable>, total=0x00007fffffffdff0) at main.cpp:25:24
    frame #10: 0x000055555563a236 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] main::$_0::operator()(this=<unavailable>) const at main.cpp:66:53
    frame #11: 0x000055555563a220 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] void std::__invoke_impl<void, main::$_0>((null)=<unavailable>, __f=<unavailable>) at invoke.h:61:14
    frame #12: 0x000055555563a220 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] std::__invoke_result<main::$_0>::type std::__invoke<main::$_0>(__fn=<unavailable>) at invoke.h:96:14
    frame #13: 0x000055555563a220 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] void std::thread::_Invoker<std::tuple<main::$_0>>::_M_invoke<0ul>(this=<unavailable>, (null)=<unavailable>) at std_thread.h:292:13
    frame #14: 0x000055555563a220 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run() [inlined] std::thread::_Invoker<std::tuple<main::$_0>>::operator()(this=<unavailable>) at std_thread.h:299:11
    frame #15: 0x000055555563a220 embed_test`std::thread::_State_impl<std::thread::_Invoker<std::tuple<main::$_0>>>::_M_run(this=<unavailable>) at std_thread.h:244:13
    frame #16: 0x00007ffff72ecdb4 libstdc++.so.6`___lldb_unnamed_symbol8048 + 20
    frame #17: 0x00005555555b8d93 embed_test`__tsan_thread_start_func + 179
    frame #18: 0x00007ffff6e9caa4 libc.so.6`___lldb_unnamed_symbol3670 + 900
    frame #19: 0x00007ffff6f29c6c libc.so.6`___lldb_unnamed_symbol4062 + 71

```

Which is call to `attr`, not `import`. So, after import called once for the module it's seems cached and safe to be called from different threads.


The issue with the first call though is: once one thread release the GIL, another could acuire it and start loading it's imports. This lead to "simultanious" modues importing from different thread and could lead to `_DeadlockError: deadlock detected by _ModuleLock` at least in case different thread trying to import different python code with different imports sets.