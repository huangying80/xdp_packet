# **xdp_packet**

huangying, email: hy_gzr@163.com

因为dpdk需要用uio驱动接管网卡，当应用出现问题时，维护起来比较麻烦，可能需要两块网卡，一块用于收发包处理，一个用于远程登陆 但是xdp可以将ebpf程序加载到网卡驱动中执行，而不需要从内核中将网卡接管。利用此特性给维护提供了方便，不需要额外的管理网卡就可以远程登录。 dpdk中虽然也支持af_xdp,但是低版本中存在个别的bug,在一定条件下会触发段错误，在低版本中会存在性能不够的情况（这些问题，dpdk官方已经有补丁） 又因为dpdk强大的功能导致整个源码复杂庞大，不太方便学习，所以决定搞一个简单点的，基于xdp的高性能收发包框架xdp_packet,以便于学习和应用到之后 的工作中，此项目需要了解xdp,ebpf为基础

此开源项目利用XDP中的af_xdp实现了网络包绕过了内核协议栈直接到用户空间的处理，且不需要像dpdk那样，需要用uio驱动接管内核的网卡驱动 此羡慕目前刚刚开始，正处于开发阶段，编译完成，但还没有经过任何调试，所以会存在许多bug.

同时非常感谢dpdk提供的支持和参考以及宝贵的系统经验！

#### 修改历史：
  **已经移动到Changelog.md**<br>
**目前为初步调试阶段,developing分支为正在调试及开发的分支**<br>
#### 当前问题：
  * 调用fgets时出现段错误，这个问题比较奇怪，至今没找到原因,栈信息如下, 目前怀疑是内存出现越界或覆盖的问题
  ```
  (gdb) bt
#0  0x00007fac44dcf2f0 in _int_malloc () from /lib64/libc.so.6
#1  0x00007fac44dd02be in malloc () from /lib64/libc.so.6
#2  0x00007fac44dbaab0 in _IO_file_doallocate () from /lib64/libc.so.6
#3  0x00007fac44dc8c60 in _IO_doallocbuf () from /lib64/libc.so.6
#4  0x00007fac44dc7c84 in __GI__IO_file_underflow () from /lib64/libc.so.6
#5  0x00007fac44dc8d16 in _IO_default_uflow () from /lib64/libc.so.6
#6  0x00007fac44dbc37a in _IO_getline_info () from /lib64/libc.so.6
#7  0x00007fac44dbb37f in fgets () from /lib64/libc.so.6
#8  0x000000000041ac23 in xdp_parse_cpu_id (val=<synthetic pointer>, filename=0x7fffdbb8ce30 "/sys/devices/system/cpu/cpu0/topology/core_id")
    at xdp_worker.c:365
#9  xdp_cpu_core_id (core_id=core_id@entry=0) at xdp_worker.c:347
#10 0x000000000041adde in xdp_workers_init () at xdp_worker.c:76
#11 0x00000000004193f3 in xdp_runtime_setup_workers (runtime=0x7fffdbb8df80, worker_func=0x404e74 <DnsProcess::worker(void volatile*)>, 
    worker_count=1) at xdp_runtime.c:177
#12 0x0000000000404713 in main (argc=9, argv=0x7fffdbb8e0e8) at main.cpp:76
  ```
经过几天的时间此问题终于解决，又是因为一个低级错误造成的内存越界
具体是在xdp_framepool.c文件中的xdp_framepool_create中给frame_list分配的空间是sizeof(struct xdp_frame * ),但在访问frame_list的时候是按sizeof(struct xdp_frame * ) * ring_size进行地址访问的，所以造成了内存越界，被覆盖的内存正好是fgets中被使用了，所以造成在调用fgets时出现段错误
