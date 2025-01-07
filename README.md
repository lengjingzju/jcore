# JCore 核心开发库说明

JCore致力于提供C语言最实用的数据结构、工具、算法。目前已提供一个内存调试工具，采用代码注入的方法，无需重新编译原始程序(动态链接)，即可调试它的内存问题，也提供库函数替换法重新编译程序替换库函数。

## jhook内存调试工具

* 内存调试工具jhook仅用约800行核心代码就实现了valgrind约26000行代码实现的memcheck的核心功能，无需重新编译程序即可完成内存越界分析和内存泄漏分析。
    * 代码注入的实现原理是使用 `LD_PRELOAD` 在程序运行前优先加载的指定的动态库，这个库中重新定义堆分配释放函数，加入调试和统计钩子。
    * 内存越界分析的核心思想是分配内存时多分配一些内存，在尾部内存处memset成校验字节，如果检测到尾部校验字节不对，就判定为内存有越界。
    * 内存泄漏分析的核心思想是以“分配大小和函数调用栈”为锚点，分配的内存指针挂载在锚点上，从而统计指定锚点分配了多少次，释放了多少次，如果“分配减释放”的值一直在增大，则判定该位置存在内存泄漏。
* 内存调试工具jhook根据记录统计数据的数据结构，有两个实现，两者的功能和接口是完全一致的，选择其中一个实现即可。
    * `libjlisthook.so`: 由 `jlisthook.c` `jhook.c` `jlist.h` 编译生成，使用单向循环列表记录统计数据，节点占用内存空间较小，但有大量分配数据时记录统计信息较慢。
    * `libjtreehook.so`: 由 `jtreehook.c` `jhook.c` `jtree.h`  `jtree.c` 编译生成，使用红黑树记录统计数据，节点占用内存空间较大，但有大量分配数据时记录统计信息较快，统计信息打印时按分配单元大小从小到大排序。

注：用户也可以使用开源工具 `valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all <bin>` 命令查找内存泄漏。

* 使用jhook检测的内存泄漏

```c
$ make O=obj check_cycle=10 check_unwind=y check_depth=10 # 编译选项
$ LD_PRELOAD=/home/lengjing/data/jcore/obj/libjtreehook.so ./yyjson twitter.json 1
[mut] read=3     parse=3     format=3     unformat=2
--------------------------------------------------------
size     alloc    free     diff     addr
1024     1        0        1        0x7f936fd1dd04:(_IO_file_doallocate+0x94) | 0x7f936fd2ded0:(_IO_doallocbuf+0x50) | 0x7f936fd2cf30:(_IO_file_overflow+0x1b0) | 0x7f936fd2b6b5:(_IO_file_xsputn+0xe5) | 0x7f936fd12972:(psiginfo+0x13512) | 0x7f936fdca27b:(__printf_chk+0xab) | 0x55895411646f:(main+0x20f) | 0x7f936fcc0083:(__libc_start_main+0xf3) | 0x55895411663e:(_start+0x2e)
631518   1        0        1        0x55895411d290:(yyjson_read_opts+0xb0) | 0x558954116358:(main+0xf8) | 0x7f936fcc0083:(__libc_start_main+0xf3) | 0x55895411663e:(_start+0x2e)
631632   1        0        1        0x55895411d64e:(yyjson_read_opts+0x46e) | 0x558954116358:(main+0xf8) | 0x7f936fcc0083:(__libc_start_main+0xf3) | 0x55895411663e:(_start+0x2e)
----------- total=1264174    peak=3894705    -----------
```

* 使用valgrind检测的内存泄漏

```
$ valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./yyjson twitter.json 1
==240599== Memcheck, a memory error detector
==240599== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==240599== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==240599== Command: ./yyjson twitter.json 1
==240599==
[mut] read=14    parse=27    format=19    unformat=14
==240599==
==240599== HEAP SUMMARY:
==240599==     in use at exit: 1,263,150 bytes in 2 blocks
==240599==   total heap usage: 57 allocs, 55 frees, 7,069,897 bytes allocated
==240599==
==240599== 631,518 bytes in 1 blocks are indirectly lost in loss record 1 of 2
==240599==    at 0x483B7F3: malloc (in /usr/lib/x86_64-linux-gnu/valgrind/vgpreload_memcheck-amd64-linux.so)
==240599==    by 0x11028F: yyjson_read_opts (in /home/lengjing/data/test/yyjson)
==240599==    by 0x109357: main (in /home/lengjing/data/test/yyjson)
==240599==
==240599== 1,263,150 (631,632 direct, 631,518 indirect) bytes in 1 blocks are definitely lost in loss record 2 of 2
==240599==    at 0x483B7F3: malloc (in /usr/lib/x86_64-linux-gnu/valgrind/vgpreload_memcheck-amd64-linux.so)
==240599==    by 0x11064D: yyjson_read_opts (in /home/lengjing/data/test/yyjson)
==240599==    by 0x109357: main (in /home/lengjing/data/test/yyjson)
==240599==
==240599== LEAK SUMMARY:
==240599==    definitely lost: 631,632 bytes in 1 blocks
==240599==    indirectly lost: 631,518 bytes in 1 blocks
==240599==      possibly lost: 0 bytes in 0 blocks
==240599==    still reachable: 0 bytes in 0 blocks
==240599==         suppressed: 0 bytes in 0 blocks
==240599==
==240599== For lists of detected and suppressed errors, rerun with: -s
==240599== ERROR SUMMARY: 1 errors from 1 contexts (suppressed: 0 from 0)
```

### jhook编译方式

```sh
make O=obj check_cycle=10 check_tofile=n check_unwind=n check_depth=1
```

* [IMake](https://github.com/lengjingzju/cbuild-ng) 变量
    * O=xxx: 指定编译输出目录
    * DESTDIR=xxx: 指定安装目录
    * DEPDIR=xxx: 指定依赖包的根目录
* 私有变量
    * check_cycle=N: check_cycle为几秒检测一次，默认为10s
        * make传入此变量会创建一个线程每N秒检测一次内存情况，否则线程空转不检测
    * check_tofile=<y|n>: 输出到文件还是终端，默认是n，输出到文件
        * 设置为 `y` 时输出到文件 `heap_memory_info.<pid>.log` 而不是终端
    * check_unwind=<y|n>: 是否使用libunwind记录栈信息，默认是n，没有依赖其它包，不会记录函数名和偏移量
        * 设置为 `y` 时需要依赖 [libunwind](https://github.com/libunwind/libunwind)
    * check_depth=N: 记录的调用栈深度，默认值为1
        * 如果check_unwind未设置为y，check_depth为1时使用__builtin_return_address记录，大于1时使用backtrace记录

### jhook接口说明

* `jhook_init`

```c
/**
 * @brief   初始化内存调试管理结构
 * @param   tail_num [IN] 尾部校验字节的个数，最大256，用于检测是否越界，可设置为0表示不检测越界
 * @return  成功返回0; 失败返回-1
 * @note    无
 */
int jhook_init(int tail_num);
```

* `jhook_uninit`

```c
/**
 * @brief   反初始化内存调试管理结构
 * @param   无参数
 * @return  无返回值
 * @note    无
 */
void jhook_uninit(void);
```

* `jhook_start`

```c
/**
 * @brief   开始内存调试
 * @param   无参数
 * @return  无返回值
 * @note    开始内存调试才会记录内存分配信息
 */
void jhook_start(void);
```

* `jhook_stop`

```c
/**
 * @brief   停止内存调试
 * @param   无参数
 * @return  无返回值
 * @note    停止内存调试会清空所有内存分配信息
 */
void jhook_stop(void);
```

* `jhook_check_bound`

```c
/**
 * @brief   检查是否有内存越界
 * @param   无参数
 * @return  无返回值
 * @note    此功能jhook_init时需要传入非0的tail_num
 */
void jhook_check_bound(void);
```

* `jhook_check_leak`

```c
/**
 * @brief   检查内存使用情况，查看是否有内存泄漏
 * @param   choice [IN] 检测打印节点信息的方式：0 未释放且有变化; 1 有变化; 2 未释放; 3 所有;
 *              >3 未释放数大于choice; < 0 未释放数大于-choice且有变化
 * @return  无返回值
 * @note    "未释放"指malloc和free不相等，"有变化"指这段时间有过alloc和free
 */
void jhook_check_leak(int choice);
```

* `jhook_set_flag`

```c
/**
 * @brief   修改高频检测选项
 * @param   dup_flag [IN] 是否高频检测当前的ptr和记录的ptr重复，即使用了debug的接口分配了内存，但没有使用jheap_free_debug释放
 * @param   bound_flag [IN] 是否高频检测记录的内存有越界
 * @return  无返回值
 * @note    高频检测指每次分配内存时都检测，显著降低性能
 */
void jhook_set_flag(int dup_flag, int bound_flag);
```

* `jhook_set_check`

```c
/**
 * @brief   修改线程检测内存方式
 * @param   check_flag [IN] 是否在线程中检测内存情况
 * @param   check_count [IN] 线程中多长时间检测一次内存情况，为0时不修改
 * @return  无返回值
 * @note    线程中检测每 10ms*check_count 检测一次
 */
void jhook_set_check(int check_flag, int check_count);
```

* `jhook_set_method`

```c
/**
 * @brief   修改记录栈调用的方法
 * @param   method [IN] 栈调用记录方式，0: 使用__builtin_return_address记录; 1: 使用backtrace记录
 * @return  无返回值
 * @note    __builtin_return_address: 只记录1层栈调用，速度较快，但对应C++来说，1层调用都是new，不好分析
 *          backtrace: 只记录2层调用，速度较较慢，由于backtrace内部调用malloc，需要临时关闭记录，可能造成记录遗漏
 */
void jhook_set_method(int method);
```

* `jhook_set_limit`

```c
/**
 * @brief   修改要跟踪的内存的范围
 * @param   min_limit [IN] 纳入统计的堆内存分配大小下限
 * @param   max_limit [IN] 纳入统计的堆内存分配大小上限
 * @return  无返回值
 * @note    修改范围只跟踪指定范围大小的内存，提升性能
 */
void jhook_set_limit(size_t min_limit, size_t max_limit);
```

### jhook举例说明(直接运行)

* jhook检查可以不用gdb，直接监测内存的变化，如下例子编译运行

```sh
$ make O=obj check_cycle=10
$ LD_PRELOAD=obj/libjtreehook.so curl www.bing.com
--------------------------------------------------------
size     alloc    free     diff     addr
12       15       1        14       0x7f88e131438f
16       1        0        1        0x7f88e13b8d1f
16       3        0        3        0x7f88e13b915a
20       1        0        1        0x7f88e13b8e45
22       2        0        2        0x7f88e13b8e45
23       3        0        3        0x7f88e13b8e45
24       3        0        3        0x7f88e138ef94
24       3        0        3        0x7f88e13b890e
24       1        0        1        0x7f88e13b8e45
25       3        0        3        0x7f88e13b8e45
26       1        0        1        0x7f88e13b8e45
32       1        0        1        0x7f88e1313ca0
38       1        0        1        0x7f88e15685b8
38       1        0        1        0x7f88e157a5df
40       1        0        1        0x7f88e15685b8
40       1        0        1        0x7f88e157a5df
48       1        0        1        0x7f88e15685b8
48       1        0        1        0x7f88e157a5df
51       4        0        4        0x7f88e13b83f7
52       2        0        2        0x7f88e13b83f7
54       10       0        10       0x7f88e13b83f7
56       2        0        2        0x7f88e13b83f7
56       1        0        1        0x7f88e156ac4a
62       1        0        1        0x7f88e13b83f7
72       2        0        2        0x7f88e156ac4a
80       2        0        2        0x7f88e12a6a4f
88       2        0        2        0x7f88e12a6a4f
88       1        0        1        0x7f88e13b6d7f
104      4        0        4        0x7f88e12a6a4f
112      2        0        2        0x7f88e12a6a4f
116      1        0        1        0x7f88e1313f55
120      2        0        2        0x7f88e12a6a4f
120      2        0        2        0x7f88e12a737a
168      2        0        2        0x7f88e12a6a4f
192      2        0        2        0x7f88e12a6a4f
192      2        0        2        0x7f88e156e31d
216      2        0        2        0x7f88e12a6a4f
240      1        0        1        0x7f88e156e31d
281      1        0        1        0x7f88e12a55c8
336      1        0        1        0x7f88e156f9db
432      2        0        2        0x7f88e12a6a4f
776      1        0        1        0x7f88e12a6a4f
784      1        0        1        0x7f88e12a6a4f
1200     1        0        1        0x7f88e1568284
1202     1        0        1        0x7f88e1568284
1210     1        0        1        0x7f88e1568284
1336     2        0        2        0x7f88e12a6a4f
1560     1        0        1        0x7f88e13b2775
----------- total=17485      peak=336755     -----------
```

* 使用libunwind记录栈信息

```
$ make O=obj check_cycle=10 check_unwind=y check_depth=2
$ LD_PRELOAD=obj/libjtreehook.so curl www.bing.com
--------------------------------------------------------
size     alloc    free     diff     addr
12       12       0        12       0x7f4ba141038f:(__strdup+0x1f) | 0x7f4ba13a1948:(setlocale+0x268)
12       2        0        2        0x7f4ba141038f:(__strdup+0x1f) | 0x7f4ba13a3395:(setlocale+0x1cb5)
16       1        0        1        0x7f4ba14b4d1f:(__nss_database_lookup2+0x25f) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
16       3        0        3        0x7f4ba14b515a:(__nss_lookup_function+0xca) | 0x7f4ba14754bf:(sched_setaffinity+0x1c7f)
20       1        0        1        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
22       2        0        2        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
23       3        0        3        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
24       3        0        3        0x7f4ba148af94:(tsearch+0x1c4) | 0x7f4ba14b50f1:(__nss_lookup_function+0x61)
24       3        0        3        0x7f4ba14b490e:(__gai_sigqueue+0x68e) | 0x7f4ba14b5179:(__nss_lookup_function+0xe9)
24       1        0        1        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
25       3        0        3        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
26       1        0        1        0x7f4ba14b4e45:(__nss_database_lookup2+0x385) | 0x7f4ba147627c:(sched_setaffinity+0x2a3c)
32       1        0        1        0x7f4ba140fca0:(__libc_dynarray_emplace_enlarge+0xe0) | 0x7f4ba14b3faf:(__resolv_context_put+0x138f)
38       1        0        1        0x7f4ba16655b8:(_dl_rtld_di_serinfo+0x2528) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
38       1        0        1        0x7f4ba16775df:(_dl_catch_error+0x189f) | 0x7f4ba1671a92:(_dl_exception_free+0x832)
40       1        0        1        0x7f4ba16655b8:(_dl_rtld_di_serinfo+0x2528) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
40       1        0        1        0x7f4ba16775df:(_dl_catch_error+0x189f) | 0x7f4ba1671a92:(_dl_exception_free+0x832)
48       1        0        1        0x7f4ba16655b8:(_dl_rtld_di_serinfo+0x2528) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
48       1        0        1        0x7f4ba16775df:(_dl_catch_error+0x189f) | 0x7f4ba1671a92:(_dl_exception_free+0x832)
51       4        0        4        0x7f4ba14b43f7:(__gai_sigqueue+0x177) | 0x7f4ba14b4e6c:(__nss_database_lookup2+0x3ac)
52       2        0        2        0x7f4ba14b43f7:(__gai_sigqueue+0x177) | 0x7f4ba14b4e6c:(__nss_database_lookup2+0x3ac)
54       10       0        10       0x7f4ba14b43f7:(__gai_sigqueue+0x177) | 0x7f4ba14b4e6c:(__nss_database_lookup2+0x3ac)
56       2        0        2        0x7f4ba14b43f7:(__gai_sigqueue+0x177) | 0x7f4ba14b4e6c:(__nss_database_lookup2+0x3ac)
56       1        0        1        0x7f4ba1667c4a:(_dl_rtld_di_serinfo+0x4bba) | 0x7f4ba166ddb0:(_dl_find_dso_for_object+0x920)
62       1        0        1        0x7f4ba14b43f7:(__gai_sigqueue+0x177) | 0x7f4ba14b4e6c:(__nss_database_lookup2+0x3ac)
72       2        0        2        0x7f4ba1667c4a:(_dl_rtld_di_serinfo+0x4bba) | 0x7f4ba166ddb0:(_dl_find_dso_for_object+0x920)
80       2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
88       2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
88       1        0        1        0x7f4ba14b2d7f:(__resolv_context_put+0x15f) | 0x7f4ba14b3428:(__resolv_context_put+0x808)
104      4        0        4        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
112      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
116      1        0        1        0x7f4ba140ff55:(__libc_alloc_buffer_allocate+0x15) | 0x7f4ba14b3849:(__resolv_context_put+0xc29)
120      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
120      2        0        2        0x7f4ba13a337a:(setlocale+0x1c9a) | 0x7f4ba13a21ce:(setlocale+0xaee)
168      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
192      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
192      2        0        2        0x7f4ba166b31d:(_dl_debug_state+0x113d) | 0x7f4ba166e0fd:(_dl_find_dso_for_object+0xc6d)
216      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
240      1        0        1        0x7f4ba166b31d:(_dl_debug_state+0x113d) | 0x7f4ba166e0fd:(_dl_find_dso_for_object+0xc6d)
281      1        0        1        0x7f4ba13a15c8:(__gconv_destroy_spec+0x178) | 0x7f4ba13a1e58:(setlocale+0x778)
352      1        0        1        0x7f4ba166c9db:(_dl_allocate_tls+0x2b) | 0x7f4ba156c323:(pthread_create+0xa53)
432      2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
776      1        0        1        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
784      1        0        1        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
1200     1        0        1        0x7f4ba1665284:(_dl_rtld_di_serinfo+0x21f4) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
1202     1        0        1        0x7f4ba1665284:(_dl_rtld_di_serinfo+0x21f4) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
1210     1        0        1        0x7f4ba1665284:(_dl_rtld_di_serinfo+0x21f4) | 0x7f4ba165ee97:(_dl_catch_error+0x0)
1336     2        0        2        0x7f4ba13a2a4f:(setlocale+0x136f) | 0x7f4ba13a33f4:(setlocale+0x1d14)
1560     1        0        1        0x7f4ba14ae775:(__idna_from_dns_encoding+0x925) | 0x7f4ba1477a69:(getaddrinfo+0xa19)
----------- total=17501      peak=336771     -----------
```

### jhook举例说明(gdb)

* 使用 `backtrace` 记录2层调用
    * 启动方式也可以如下(推荐)

    ```sh
    $ gdb ./jini_test                                   # 正常启动gdb
    (gdb) set environment LD_PRELOAD=./libjtreehook.so  # 设置环境变量
    (gdb) set args <xxx>                                # 设置程序运行的参数
    ```

```sh
$ make O=obj check_depth=2
$ gdb --args env LD_PRELOAD=./libjtreehook.so ./jini_test
...
(gdb) b main                    # 在main函数断点，准备初始化统计结构信息
Function "main" not defined.
Make breakpoint pending on future shared library load? (y or [n]) y
Breakpoint 1 (main) pending.
(gdb) r
Starting program: /usr/bin/env LD_PRELOAD=./libjtreehook.so ./jini_test
process 31255 is executing new program: /home/lengjing/data/jcore/obj/jini_test
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".

Breakpoint 1, 0x0000555555555340 in main ()
(gdb) call jhook_init(0)        # 初始化统计结构信息，0表示无需校验字节，不进行越界分析，其它值表示尾部校验字节的个数
$1 = 0
(gdb) call jhook_start()        # 开始统计
(gdb) call jhook_set_method(1)  # 修改栈调用记录方法，改为使用backtrace记录两层调用
(gdb) b jhook_delptr            # 在某个观察点设置断点，运行统计函数
Breakpoint 2 at 0x7ffff7fc35e8: file jtreehook.c, line 426.
(gdb) c
Continuing.
...
Breakpoint 2, jhook_delptr (ptr=0x816938a99eac5200) at jtreehook.c:426
426	{
(gdb) call jhook_check_leak(0)  # 统计信息，0为选项，内存泄漏主要是用选项0，看diff是否一直在增加
--------------------------------------------------------
size     alloc    free     diff     addr
2        1        0        1        0x555555555e82|0x55555555542e
2        1        0        1        0x555555555e82|0x55555555544b
3        1        0        1        0x555555555e82|0x555555555411
5        1        0        1        0x555555555cce|0x555555555411
5        1        0        1        0x555555555cce|0x5555555570cb
5        1        0        1        0x555555555e82|0x5555555570cb
6        1        0        1        0x555555555d33|0x555555555411
6        1        0        1        0x555555555d33|0x55555555542e
6        1        0        1        0x555555555d33|0x55555555544b
6        1        0        1        0x555555555d33|0x555555555468
6        1        0        1        0x555555555d33|0x555555555485
8        1        0        1        0x555555555d33|0x5555555553d7
8        1        0        1        0x555555555d33|0x5555555570cb
9        1        0        1        0x55555555667b|0x555555555351
10       1        0        1        0x555555555e82|0x555555555468
10       1        0        1        0x555555555e82|0x555555555485
13       1        0        1        0x555555555e82|0x5555555553d7
14       1        0        1        0x555555556045|0x555555555494
32       1        0        1        0x555555555cba|0x555555555411
32       1        0        1        0x555555555cba|0x5555555570cb
32       1        0        1        0x555555555d1f|0x5555555553d7
32       1        0        1        0x555555555d1f|0x555555555411
32       1        0        1        0x555555555d1f|0x55555555542e
32       1        0        1        0x555555555d1f|0x55555555544b
32       1        0        1        0x555555555d1f|0x555555555468
32       1        0        1        0x555555555d1f|0x555555555485
32       1        0        1        0x555555555d1f|0x5555555570cb
32       1        0        1        0x555555556616|0x555555555351
48       1        0        1        0x555555557122|0x555555556085
1024     1        0        1        0x7ffff7e38d04|0x7ffff7e48ed0
8192     1        0        1        0x555555557136|0x555555556085
----------- total=9708       peak=9708       -----------
(gdb) x /x 0x555555557136       # 查看第1层栈调用的位置
0x555555557136 <jfcache_open+70>:	0x24448949
(gdb) x /x 0x555555556085       # 查看第2层栈调用的位置
0x555555556085 <jini_flush+149>:	0x48c58948
(gdb) c
Continuing.

Breakpoint 2, jhook_delptr (ptr=0x55555555d4a8) at jtreehook.c:426
426	{
(gdb) call jhook_check_leak(1)  # 选项1表示距离上次统计，有过alloc/free的节点信息
--------------------------------------------------------
size     alloc    free     diff     addr
8192     1        1        0        0x555555557136|0x555555556085
----------- total=1516       peak=9708       -----------
```

* 使用 `__builtin_return_address` 记录1层调用

```sh
# 上面同样的方法，如果不运行call jhook_set_method(1)，统计结果如下:
(gdb) call jhook_check_leak(0)
--------------------------------------------------------
size     alloc    free     diff     addr
2        2        0        2        0x555555555e82
3        1        0        1        0x555555555e82
5        2        0        2        0x555555555cce
5        1        0        1        0x555555555e82
6        5        0        5        0x555555555d33
8        2        0        2        0x555555555d33
9        1        0        1        0x55555555667b
10       2        0        2        0x555555555e82
13       1        0        1        0x555555555e82
14       1        0        1        0x555555556045
32       2        0        2        0x555555555cba
32       7        0        7        0x555555555d1f
32       1        0        1        0x555555556616
48       1        0        1        0x555555557122
1024     1        0        1        0x7ffff7e38d04
8192     1        0        1        0x555555557136
----------- total=9708       peak=9708       -----------
(gdb) x /x 0x555555557136
0x555555557136 <jfcache_open+70>:	0x24448949
(gdb)
```

### 静态链接程序内存调试

静态链接程序使用库函数替换法，链接成静态可执行文件时需要加上链接选项 `-Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup -ljtreewrap -pthread`，此时会使用自定义的函数 `__wrap_xxxx` 代替库函数 `xxxx`，如果此时要使用真正的库函数，可以使用  `__real_xxxx` 。

例如静态编译 [LJSON](https://github.com/lengjingzju/json) 内存调试，可以如下编译： `gcc -o ljson json.c jnum.c json_test.c -O2 -ffunction-sections -fdata-sections -W -Wall -L/home/lengjing/data/jcore/obj -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup -ljtreewrap -pthread -lm` 。

### 上百进程的WebRTC大型服务调试

阶段一：判定哪个进程的哪个分配大小的内存有泄漏

* 编译资源占用小的调试程序

```sh
$ make O=obj check_cycle=10 check_tofile=y
```

* 导出环境变量，以代码注入方式运行程序

```sh
$ export LD_PRELOAD=<jhook_path>/libjtreehook.so
$ ./<cmd>
```

* 运行一段时间后，查看哪个记录文件表现内存一直在增长

```sh
$ grep -n 'total=' `ls -Sr <log_path>/heap_memory_info.*.log`
```

* 辅助分析手段：对文件进行如下处理，看下哪个节点的内存疑似泄漏
    * size和addr相同是同一节点，如果diff值一直增加，表明该节点可能存在内存泄漏

```sh
$ grep -E '^[0-9]+ ' heap_memory_info.<pid>.log | nl | sort -k2n -k6 -k1n | cut -f 2-
```

* 辅助信息手段：查看进程线程相关信息

```sh
$ cat /proc/<pid>/cmdline
$ top -Hp <pid>
$ ps -Tlf <pid>
```

阶段二：得到内存泄漏的节点的堆栈

* 修改调试程序源码只监测特定size的内存分配

```sh
--- a/jtreehook.c
+++ b/jtreehook.c
@@ -745,6 +745,7 @@ void before_main(void)
     jhook_mgr_t *mgr = &s_jhook_mgr;
     if (!mgr->inited) {
         jhook_init(0);
+        jhook_set_limit(<size>, <size>);
         jhook_start();
     }
 }
```

* 如果系统不带 `libunwind`，需要先下载编译 `libunwind`

```sh
$ autoreconf --install
$ mkdir build && cd build
$ ../configure --prefix=<libunwind install root>
$ make && make install
```

* 编译记录节点调用栈的函数，调用栈层数可配

```sh
$ make O=obj DEPDIR=<libunwind install root> check_cycle=10 check_tofile=y check_unwind=y check_depth=10
```

* 导出环境变量，以代码注入方式运行程序

```sh
$ export LD_LIBRARY_PATH=<libunwind.so path>:$LD_LIBRARY_PATH
$ export LD_PRELOAD=<jhook_path>/libjtreehook.so
$ ./<cmd>
```

* 运行一段时间后，查看记录文件的堆栈信息和内存泄漏信息

* 如果是 `C++` 程序，可以使用如下脚本处理文件得到真正的函数

```sh
#!/bin/bash

src=$1

if [ -z $src ]; then
    echo "Usage: $0 <src>"
    exit 1
fi

IFS=$'\n'
for line in $(cat $src | sed -e 's/ 0x/\n0x/g' -e 's/ |//g'); do
    if [ $(echo "$line" | grep -c "^0x") -eq 1 ]; then
        prefix=$(echo "$line" | cut -d '(' -f 1)
        suffix=$(echo "$line" | cut -d '(' -f 2 | cut -d '+' -f 2 | cut -d ')' -f 1)
        oldfunc=$(echo "$line" | cut -d '(' -f 2 | cut -d '+' -f 1)
        newfunc=$(c++filt $oldfunc 2>/dev/null)
        echo "$prefix $newfunc+$suffix"
    else
        if [ $(echo "$line" | grep -c "^[0-9]") -eq 1 ]; then
            echo -e "\033[33m$line\033[0m"
        else
            echo -e "\033[32m$line\033[0m"
        fi
    fi
done
```

* 最终发现下面的内存积攒来不及释放

```sh
1640     1072685  375670   697015
0x7f4f024de298: operator new(unsigned long)+0x18
0x7f4eea713bb0: std::__shared_ptr<erizo::DataPacket, (__gnu_cxx::_Lock_policy)2>::__shared_ptr<std::allocator<erizo::DataPacket>, erizo::DataPacket&>(std::_Sp_make_shared_tag, std::allocator<erizo::DataPacket> const&, erizo::DataPacket&)+0x30
0x7f4eea709c4d: erizo::MediaStream::deliverAudioData_(std::shared_ptr<erizo::DataPacket>)+0x3d
0x7f4eeb535912: owt_base::AudioFramePacketizer::onAdapterData(char*, int)+0x172
0x7f4eeb00ec55: non-virtual thunk to rtc_adapter::AudioSendAdapterImpl::SendRtp(unsigned char const*, unsigned long, webrtc::PacketOptions const&)+0x15
0x7f4eeb11c3c8: webrtc::DEPRECATED_RtpSenderEgress::SendPacketToNetwork(webrtc::RtpPacketToSend const&, webrtc::PacketOptions const&, webrtc::PacedPacketInfo const&)+0x58
0x7f4eeb11cd42: webrtc::DEPRECATED_RtpSenderEgress::SendPacket(webrtc::RtpPacketToSend*, webrtc::PacedPacketInfo const&)+0x252
0x7f4eeb11d087: webrtc::DEPRECATED_RtpSenderEgress::NonPacedPacketSender::EnqueuePackets(std::__1::vector<std::__1::unique_ptr<webrtc::RtpPacketToSend, std::__1::default_delete<webrtc::RtpPacketToSend> >, std::__1::allocator<std::__1::unique_ptr<webrtc::RtpPacketToSend, std::__1::default_delete<webrtc::RtpPacketToSend> > > >)+0x97
0x7f4eeb061e1e: webrtc::RTPSender::SendToNetwork(std::__1::unique_ptr<webrtc::RtpPacketToSend, std::__1::default_delete<webrtc::RtpPacketToSend> >)+0x14e
0x7f4eeb053569: webrtc::RTPSenderAudio::SendAudio(webrtc::AudioFrameType, signed char, unsigned int, unsigned char const*, unsigned long, long)+0x849

1640     753557   495014   258543
0x7f4f024de298: operator new(unsigned long)+0x18
0x7f4eea713bb0: std::__shared_ptr<erizo::DataPacket, (__gnu_cxx::_Lock_policy)2>::__shared_ptr<std::allocator<erizo::DataPacket>, erizo::DataPacket&>(std::_Sp_make_shared_tag, std::allocator<erizo::DataPacket> const&, erizo::DataPacket&)+0x30
0x7f4eea70a25e: erizo::MediaStream::onTransportData(std::shared_ptr<erizo::DataPacket>, erizo::Transport*)+0x4e
0x7f4eea6ec473: std::_Function_handler<void (std::shared_ptr<erizo::MediaStream> const&), erizo::WebRtcConnection::onTransportData(std::shared_ptr<erizo::DataPacket>, erizo::Transport*)::{lambda(std::shared_ptr<erizo::MediaStream> const&)#3}>::_M_invoke(std::_Any_data const&, std::shared_ptr<erizo::MediaStream> const&)+0x103
0x7f4eea6ea882: erizo::WebRtcConnection::forEachMediaStream(std::function<void (std::shared_ptr<erizo::MediaStream> const&)>)+0x62
0x7f4eea6ecb4a: erizo::WebRtcConnection::onTransportData(std::shared_ptr<erizo::DataPacket>, erizo::Transport*)+0x38a
0x7f4eea6c78f3: erizo::UnencryptedTransport::onIceData(std::shared_ptr<erizo::DataPacket>)+0x233
0x7f4eea6c3a85: std::_Function_handler<void (), erizo::Transport::onPacketReceived(std::shared_ptr<erizo::DataPacket>)::{lambda()#1}>::_M_invoke(std::_Any_data const&)+0xf5
0x7f4eea7243fd: boost::asio::detail::completion_handler<std::function<void ()> >::do_complete(boost::asio::detail::task_io_service*, boost::asio::detail::task_io_service_operation*, boost::system::error_code const&, unsigned long)+0x7d
0x7f4eea72504b: boost::asio::detail::task_io_service::run(boost::system::error_code&)+0x43b
```
