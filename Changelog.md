huangying, email: hy_gzr@163.com

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
* 2021-04-29:<br>（此次改动，基本功能以调试通过，在单核单网卡队列下已经可以正常接收和响应dns报文）
  * 解决了调用fgets时出现段错误的问题，原因是因为frame_list分配了sizeof(struct xdp_frame * ) 的大小，但实际使用时使用了sizeof(struct xdp_frame * ) * ring_size大小的空间造成了内存越界
* 2021-05-12:<br>
  * 更新了libbpf库
  * 新增文件edns.h，定义了edns相关结构，用在测试程序sample/dns_server中
  * 修改了测试程序sample/dns_server中的packet.cpp和packet.h,用以解决封装dns响应包格式不正确的问题
  * 修改了测试程序sample/dns_server中的process.cpp,解决了ip头校验和不正确的问题
  * 修改了测试程序sample/dns_server中的process.cpp,解决了一直不停发包的问题
  * 修改了xdp_ipv4.h文件，修正了对ip报文字节数检查错误的问题
  * 修改了xdp_prog.c文件，针对PERCPU_HASH类型的map更新时传入参数错误的问题，正确的参数应该是传入一个数组
  * 修改了xdp_sock.c文件，解决了从umem中获取报文地址时错误的计算了frame->data_off的问题
  * 修改了xdp_kern_prog.c文件，调整了丢包、重定向、通过等状态的逻辑,增加了XDP_NOSET状态用于表示没有设置状态<br>
    将逻辑调整成了如果没有设置就检查下一层协议，如果设置了状态，则不论是什么状态都直接返回，这么做是为了提高执行效率
* 2021-05-17:<br>
  * 修改了sample/dns_server/Makefile文件，以启用输出调试信息的功能
  * 修改了xdp_dev.h和xdp_dev.c文件，在计算收发队列数据结构占用字节数的计算中去掉了在最后加上XDP_CACHE_LINE的处理以解决过多的从内存池中分配内存造成内存池不够用的问题
  * 修改了xdp_frame.h和xdp_fram.c，在计算收发队列数据结构占用字节数的计算中去掉了在最后加上XDP_CACHE_LINE的处理以解决过多的从内存池中分配内存造成内存池不够用的问题
  * 修改了xdp_hugepage.c文件，在xdp_hugepage_init中增加了先清理环境再挂在的处理，以防止程序意外退出再启动时对大页的重复占用造成耗尽的问题
  * 修改了xdp_hugepage.c文件，在xdp_hugepage_releae中增加了对大页文件及目录是否存在的判断以避免出现umount2调用失败的情况
  * 修改了xdp_kern_prog/xdp_kern_prog.c文件，将所有BPF_MAP_TYPE_PERCPU_HASH类型map的value_size的设置，从__u32改成了__u64,因为BPF_MAP_TYPE_PERCPU_HASH类型的map的值大小必须是8字节对齐的
  * 修改了xdp_kern_prog/xdp_kern_prog.c文件，当action为XDP_NOSET时，将此action设置成XDP_PASS,也就是将默认action调整成了XDP_PASS
  * 修改了xdp_prog.c文件，增加了xdp_prog_reload接口，用来程序在上一次加载后意外退出后而没有卸载prog造成的本次无法加载的问题
  * 修改了xdp_prog.c文件，纠正了对BPF_MAP_TYPE_PERCPU_HASH类型map错误的更新方法的问题
  * 修改了xdp_ring.h和xdp_ring.c文件，在计算收发队列数据结构占用字节数的计算中去掉了在最后加上XDP_CACHE_LINE的处理以解决过多的从内存池中分配内存造成内存池不够用的问题
  * 修改了xdp_runtime.h和xdp_runtime.c文件，增加了允许用户指定numa节点的处理
  * 修改了xdp_worker.c文件，解决了使能worker时，返回使能的worker的数量不对的问题
* 2021-05-18:<br>
  * 修改了sample/dns_server/main.cpp文件，增加了命令行参数-Q,以便于在压力测试时指定队列数量
  * 修改了sample/dns_server/main.cpp文件，增加了对xdp_runtime_setup_rss_ipv4的调用，用以设置网卡RSS
  * 修改了xdp_eth.c和xdp_eth.h文件，实现了设置网卡多队列RSS的设置接口，目前支持ipv4和ipv6的源IP|目标IP|源端口|目标端口作为RSS的hash key
  * 修改了xdp_runtime.h和xdp_runtime.c,封装了设置网卡多队列RSS的接口，为外层调用
