huangying-c, email: hy_gzr@163.com

#### 修改历史：
* 2021-04-15:<br>
  * Compiles complete, but without any debugging
* 2021-04-16:<br>
  * 新加文件xdp_prefetch.h,在这个文件中定义了cpu预取的接口
  * 新加文件xdp_endian.h,在这个文件中定义了字节序转换的接口
  * 修改了ebpf程序中的bug,这个bug会造成无法获取4层协议的问题
  * 增加了对ipv6的支持
* 2021-04-20:<br>
  * 新增文件xdp_ipv4.h,在这个文件中定义了获取ipv4的头大小，包大小及对ipv4头的合法性检查
  * 修改了struct xdp_frame结构体，增加了指向framepool的指针成员
  * 新增文件xdp_ipv6.h,在这个文件中定义了检查ipv6报文头合法性的接口
  * 修改了xdp_kern_prog/xdp_kern_prog.c,在其中增加了对目的地址过滤的功能
  * 将xdp_endian.h更名为xdp_net.h,在原有基础上增加了ip头效验和及udp tcp头的效验和功能
  * 修改了xdp_prefetch.h,将xdp_prefetchX的参数去掉了volatile修饰
  * 修改了xdp_prog.c,增加了对目的IP地址的更新的功能
  * 修改了xdp_prog.c,纠正了xdp_prog_release定义不正确的错误
  * 修改了xdp_runtime.c,对xdp_runtime_setup_workers的参数进行了调整，增加了worker_func参数用于指定worker的处理函数
  * 修改了xdp_runtime.c,增加了xdp_runtime_startup_workers接口，用于启动worker线程
  * 修改了xdp_runtime.c,调整了设置IP过滤设置的相关接口的参数，使得可以支持设置目标IP
  * 修改该了xdp_worker.c及xdp_worker.h,调整了xdp_worker_start_by_id接口的访问权限，以供其他模块调用
  * 修改了一些语法错误
  * 增加了sample/dns_server目录，此目录中的代码实现了一个简单的dns服务器，目的是为了测试xdp_packet,目前已经编译完成，还未调试
* 2021-04-26:<br>
  * 修改了Makefile文件，将xdp_kern_prog.o生成在xdp_kern_prog目录下
  * 修改了Makefile文件，为了调试暂时去掉了-O2选项
  * 修改了测试程序sample/dns_server/main.cpp 增加了对命令行参数的处理
  * 修改了xdp_dev.c及xdp_dev.h文件，调整了xdp_dev_configure的参数
  * 修改了xdp_dev.c及xdp_dev.h文件，分别为struct xdp_tx_queue和struct xdp_rx_queue增加了创建实例的接口并且在适当的为止调用
  * 修改了xdp_dev.c文件，修复了在分配xdp_umem_info实例时对界限的判断的bug
  * 调整了主要数据结构，在结构中增加了cache line对齐的补偿，以提高性能
  * 调整了主要数据结构字节数的计算，增加了因地址对齐需要移动的字节数
  * 修改了xdp_eth.c文件，修复了设置网卡队列数量失败的问题
  * 修改了xdp_head.c文件，调整了mmap映射参数，解决了映射匿名内存失败的问题
  * 修改了xdp_kern_prog/xdp_kern_prog.h文件，增加了一些宏用来引用xsks_map
  * 修改了xdp_kern_prog/xdp_kern_prog.c文件，调整了重定向map的名字固定为xsks_map,这是受libbpf库的约束
  * 修改了xdp_log.h文件，解决了日志无法按预期输出的问题
  * 修改了xdp_mempool.c文件，解决了遍历mempool_ops数组时指向不正确造成段错误的问题
  * 修改了xdp_mempool.h文件，增加了cache line对齐的宏定义及判断是否2的幂和2的幂对齐的接口
  * 修改了xdp_prog.c文件，在xdp_prog_init中如果prog已经加载时返回错误
  * 修改了xdp_ring.c文件，调整了判断队列长度是否为2的幂的方式
  * 修改了xdp_sock.c文件，修复了为fill_queue分配缓存时，对返回值的错误的判断的问题
 * 2021-04-29:<br>
  * 解决了调用fgets时出现段错误的问题，原因是因为frame_list分配了sizeof(struct xdp_frame * ) 的大小，但实际使用时使用了sizeof(struct xdp_frame * ) * ring_size大小的空间造成了内存越界
