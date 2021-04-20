# **xdp_packet**

huangying, email: hy_gzr@163.com

因为dpdk需要用uio驱动接管网卡，当应用出现问题时，维护起来比较麻烦，可能需要两块网卡，一块用于收发包处理，一个用于远程登陆 但是xdp可以将ebpf程序加载到网卡驱动中执行，而不需要从内核中将网卡接管。利用此特性给维护提供了方便，不需要额外的管理网卡就可以远程登录。 dpdk中虽然也支持af_xdp,但是低版本中存在个别的bug,在一定条件下会触发段错误，在低版本中会存在性能不够的情况（这些问题，dpdk官方已经有补丁） 又因为dpdk强大的功能导致整个源码复杂庞大，不太方便学习，所以决定搞一个简单点的，基于xdp的高性能收发包框架xdp_packet,以便于学习和应用到之后 的工作中，此项目需要了解xdp,ebpf为基础

此开源项目利用XDP中的af_xdp实现了网络包绕过了内核协议栈直接到用户空间的处理，且不需要像dpdk那样，需要用uio驱动接管内核的网卡驱动 此羡慕目前刚刚开始，正处于开发阶段，编译完成，但还没有经过任何调试，所以会存在许多bug.

同时非常感谢dpdk提供的支持和参考以及宝贵的系统经验！

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
