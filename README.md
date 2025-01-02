# JCore 核心开发库说明

JCore致力于提供C语言最实用的数据结构、工具、算法。目前已提供一个内存调试工具，采用代码注入的方法，无需重新编译原始程序(动态链接)，即可调试它的内存问题，也提供库函数替换法重新编译程序替换库函数。

## 内存调试工具

* 内存调试工具jhook仅用约500行代码就实现了valgrind约26000行代码实现的memcheck的核心功能，无需重新编译程序即可完成内存越界分析和内存泄漏分析。
    * 代码注入的实现原理是使用 `LD_PRELOAD` 在程序运行前优先加载的指定的动态库，这个库中重新定义堆分配释放函数，加入调试和统计钩子。
    * 内存越界分析的核心思想是分配内存时多分配一些内存，在尾部内存处memset成校验字节，如果检测到尾部校验字节不对，就判定为内存有越界。
    * 内存泄漏分析的核心思想是以“分配大小和函数调用栈”为锚点，分配的内存指针挂载在锚点上，从而统计指定锚点分配了多少次，释放了多少次，如果“分配减释放”的值一直在增大，则判定该位置存在内存泄漏。

* 内存调试工具jhook根据记录统计数据的数据结构，有两个实现，两者的功能和接口是完全一致的，选择其中一个实现即可。
    * `libjlisthook.so`: 由 `jlisthook.c` `jhook.c` `jlist.h` 编译生成，使用单向循环列表记录统计数据，节点占用内存空间较小，但有大量分配数据时记录统计信息较慢。
    * `libjtreehook.so`: 由 `jtreehook.c` `jhook.c` `jtree.h`  `jtree.c` 编译生成，使用红黑树记录统计数据，节点占用内存空间较大，但有大量分配数据时记录统计信息较快，统计信息打印时按分配单元大小从小到大排序。

注：用户也可以使用开源工具 `valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all <bin>` 命令查找内存泄漏.

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
$ make O=obj check_cycle=10 # check_cycle为几秒检测一次
$ LD_PRELOAD=obj/libjtreehook.so curl www.bing.com
--------------------------------------------------------
size     alloc    free     diff     addr
12       15       1        14       0x7f88e131438f|(nil)
16       1        0        1        0x7f88e13b8d1f|(nil)
16       3        0        3        0x7f88e13b915a|(nil)
20       1        0        1        0x7f88e13b8e45|(nil)
22       2        0        2        0x7f88e13b8e45|(nil)
23       3        0        3        0x7f88e13b8e45|(nil)
24       3        0        3        0x7f88e138ef94|(nil)
24       3        0        3        0x7f88e13b890e|(nil)
24       1        0        1        0x7f88e13b8e45|(nil)
25       3        0        3        0x7f88e13b8e45|(nil)
26       1        0        1        0x7f88e13b8e45|(nil)
32       1        0        1        0x7f88e1313ca0|(nil)
38       1        0        1        0x7f88e15685b8|(nil)
38       1        0        1        0x7f88e157a5df|(nil)
40       1        0        1        0x7f88e15685b8|(nil)
40       1        0        1        0x7f88e157a5df|(nil)
48       1        0        1        0x7f88e15685b8|(nil)
48       1        0        1        0x7f88e157a5df|(nil)
51       4        0        4        0x7f88e13b83f7|(nil)
52       2        0        2        0x7f88e13b83f7|(nil)
54       10       0        10       0x7f88e13b83f7|(nil)
56       2        0        2        0x7f88e13b83f7|(nil)
56       1        0        1        0x7f88e156ac4a|(nil)
62       1        0        1        0x7f88e13b83f7|(nil)
72       2        0        2        0x7f88e156ac4a|(nil)
80       2        0        2        0x7f88e12a6a4f|(nil)
88       2        0        2        0x7f88e12a6a4f|(nil)
88       1        0        1        0x7f88e13b6d7f|(nil)
104      4        0        4        0x7f88e12a6a4f|(nil)
112      2        0        2        0x7f88e12a6a4f|(nil)
116      1        0        1        0x7f88e1313f55|(nil)
120      2        0        2        0x7f88e12a6a4f|(nil)
120      2        0        2        0x7f88e12a737a|(nil)
168      2        0        2        0x7f88e12a6a4f|(nil)
192      2        0        2        0x7f88e12a6a4f|(nil)
192      2        0        2        0x7f88e156e31d|(nil)
216      2        0        2        0x7f88e12a6a4f|(nil)
240      1        0        1        0x7f88e156e31d|(nil)
281      1        0        1        0x7f88e12a55c8|(nil)
336      1        0        1        0x7f88e156f9db|(nil)
432      2        0        2        0x7f88e12a6a4f|(nil)
776      1        0        1        0x7f88e12a6a4f|(nil)
784      1        0        1        0x7f88e12a6a4f|(nil)
1200     1        0        1        0x7f88e1568284|(nil)
1202     1        0        1        0x7f88e1568284|(nil)
1210     1        0        1        0x7f88e1568284|(nil)
1336     2        0        2        0x7f88e12a6a4f|(nil)
1560     1        0        1        0x7f88e13b2775|(nil)
----------- total=17485      peak=336755     -----------
```

### jhook举例说明(gdb)

* 使用 `backtrace` 记录2层调用
    * 启动方式也可以如下

    ```sh
    $ gdb ./jini_test                                   # 正常启动gdb
    (gdb) set environment LD_PRELOAD=./libjtreehook.so  # 设置环境变量
    (gdb) set args <xxx>                                # 设置程序运行的参数
    ```

```sh
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
2        2        0        2        0x555555555e82|(nil)
3        1        0        1        0x555555555e82|(nil)
5        2        0        2        0x555555555cce|(nil)
5        1        0        1        0x555555555e82|(nil)
6        5        0        5        0x555555555d33|(nil)
8        2        0        2        0x555555555d33|(nil)
9        1        0        1        0x55555555667b|(nil)
10       2        0        2        0x555555555e82|(nil)
13       1        0        1        0x555555555e82|(nil)
14       1        0        1        0x555555556045|(nil)
32       2        0        2        0x555555555cba|(nil)
32       7        0        7        0x555555555d1f|(nil)
32       1        0        1        0x555555556616|(nil)
48       1        0        1        0x555555557122|(nil)
1024     1        0        1        0x7ffff7e38d04|(nil)
8192     1        0        1        0x555555557136|(nil)
----------- total=9708       peak=9708       -----------
(gdb) x /x 0x555555557136
0x555555557136 <jfcache_open+70>:	0x24448949
(gdb)
```

### 静态链接程序内存调试

静态链接程序使用库函数替换法，链接成静态可执行文件时需要加上链接选项 `-Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup -ljtreewrap -pthread`，此时会使用自定义的函数 `__wrap_xxxx` 代替库函数 `xxxx`，如果此时要使用真正的库函数，可以使用  `__real_xxxx` 。

例如静态编译 [LJSON](https://github.com/lengjingzju/json) 内存调试，可以如下编译： `gcc -o ljson json.c jnum.c json_test.c -O2 -ffunction-sections -fdata-sections -W -Wall -L/home/lengjing/data/jcore/obj -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=strdup -Wl,--wrap=strndup -ljtreewrap -pthread -lm` 。
