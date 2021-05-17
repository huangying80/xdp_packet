**xdp_packet**<br>
huangying, email: hy_gzr@163.com
====

因为dpdk需要用uio驱动接管网卡，当应用出现问题时，维护起来比较麻烦，可能需要两块网卡，一块用于收发包处理，一个用于远程登陆 但是xdp可以将ebpf程序加载到网卡驱动中执行，而不需要从内核中将网卡接管。利用此特性给维护提供了方便，不需要额外的管理网卡就可以远程登录。 dpdk中虽然也支持af_xdp,但是低版本中存在个别的bug,在一定条件下会触发段错误，在低版本中会存在性能不够的情况（这些问题，dpdk官方已经有补丁） 又因为dpdk强大的功能导致整个源码复杂庞大，不太方便学习，所以决定搞一个简单点的，基于xdp的高性能收发包框架xdp_packet,以便于学习和应用到之后 的工作中，此项目需要了解xdp,ebpf为基础

此开源项目利用XDP中的af_xdp实现了网络包绕过了内核协议栈直接到用户空间的处理，且不需要像dpdk那样，需要用uio驱动接管内核的网卡驱动 此羡慕目前刚刚开始，正处于开发阶段，编译完成，但还没有经过任何调试，所以会存在许多bug.

同时非常感谢dpdk提供的支持和参考以及宝贵的系统经验！

#### 修改历史：
  **已经移动到Changelog.md**<br>
**目前为初步调试阶段,developing分支为正在调试及开发的分支**<br>
#### 当前状态：
  * 大页内存功能调试完成
#### 未来计划：
  * ~~单核单网卡队列收发包功能调试~~
  * ~~大页内存功能调试~~
  * 调试在多核多网卡队列下的收发包处理
  * 单核单网卡下的测试程序sample/dns_server的压力测试
  * 单核单网卡下且设置numa节点的测试程序sample/dns_server的压力测试
  * 多核多网卡下的测试程序sample/dns_server的压力测试
  * 多核多网卡下且设置numa节点的测试程序sample/dns_server的压力测试
#### 当前问题：
  * 当启用两个网卡队列时，只能从一个队列上收到包.`解决中`
  ____
  * 另外一个队列在查询action时，是一个大于XDP_NOSET的异常值,目前怀疑是设置action时的问题.`已解决`
  ```
       xdp_wrk_1-3690    [001] d.s. 4841387.931197: bpf_trace_printk: udp port 53, action 4
       xdp_wrk_1-3690    [001] d.s. 4841395.431245: bpf_trace_printk: udp port 53, action 4
          <idle>-0       [019] d.s. 4841399.366956: bpf_trace_printk: udp port 53, action 4343432
          <idle>-0       [019] dNs. 4841401.367064: bpf_trace_printk: udp port 53, action 4343432
          <idle>-0       [019] dNs. 4841403.367243: bpf_trace_printk: udp port 53, action 4343432
          <idle>-0       [019] d.s. 4841415.952493: bpf_trace_printk: udp port 53, action 4343432
          <idle>-0       [019] dNs. 4841417.952617: bpf_trace_printk: udp port 53, action 4343432
          <idle>-0       [019] dNs. 4841419.952769: bpf_trace_printk: udp port 53, action 4343432
   ```
   通过分析内核源码关于系统调用bpf的实现可以确定是使用BPF_MAP_TYPE_PERCPU_HASH的方法不对造成的内存被污染的问题，造成在更新及查询此类map时无法正确获得指定key对应的value
   ** 首先分析内核中更新操作的代码
   涉及文件:<br>
   kernel/bpf/syscall.c<br>
   调用过程：<br>
   SYSCALL_DEFINE3(bpf, ...) -> map_update_elem<br>
   相关代码：<br>
   ```
   SYSCALL_DEFINE3(bpf, int, cmd, union bpf_attr __ user \*, uattr , unsigned int , size)
   {
     ....
     switch (cmd) {
     case BPF_MAP_CREATE:
        err = map_create(&attr);
        break;
     case BPF_MAP_LOOKUP_ELEM:
        err = map_lookup_elem(&attr);
        break;
     case BPF_MAP_UPDATE_ELEM:
        err = map_update_elem(&attr);//此处执行真正的更新操作
        break;
     ...
   }
  
   static int map_update_elem(union bpf_attr *attr)
   {
     ...
     if (map->map_type == BPF_MAP_TYPE_PERCPU_HASH ||
        map->map_type == BPF_MAP_TYPE_LRU_PERCPU_HASH ||
        map->map_type == BPF_MAP_TYPE_PERCPU_ARRAY ||
        map->map_type == BPF_MAP_TYPE_PERCPU_CGROUP_STORAGE)
        value_size = round_up(map->value_size, 8) * num_possible_cpus();//此处对于per cpu类的map的value_size必须是8字节对齐乘以cpu核数
     else
        value_size = map->value_size;
     
     err = -ENOMEM;
     value = kmalloc(value_size, GFP_USER | __GFP_NOWARN);
     if (!value)
         goto free_key;

     err = -EFAULT;
     if (copy_from_user(value, uvalue, value_size) != 0)//此处将value_size个字节数从用户空间拷贝到内核空间
        goto free_value;

     err = bpf_map_update_value(map, f, key, value, attr->flags);
     ...
   }
   ```
   所以要求来自用户空间的value的字节数必须是cpu核数个的长度，每个元素大小为8字节对齐的连续空间<br>
   结论<br>
   1. 也就是说在定义BPF_MAP_TYPE_PERCPU_HASH的map时value的大小必须是8字节对齐的，如果是数值型的只能是uint64_t等8字节的数据类型
   2. 再更新时必须是cpu核心数的长度的数组
   例：
   ```
   struct bpf_map_def SEC("maps") percpu_map {
    .type = BPF_MAP_TYPE_PERCPU_HASH,
    .key_size = sizeof(uint32_t),
    .value_size = sizeof(uint64_t), //此处value_size必须为8对齐
    .max_entries = 1024,
   }
   //更新示例代码
   static int xdp_update_map_percpu(int map_fd, __u32 key, __u32 val)
   {
        int          err;
        unsigned int i;
        unsigned int nr_cpus = libbpf_num_possible_cpus();
        __u64        cpus_val[nr_cpus];
        for (i = 0; i < nr_cpus; i++) {
            cpus_val[i] = val;
        }
        err = bpf_map_update_elem(map_fd, &key, cpus_val, BPF_ANY);
        if (err < 0) {
            ERR_OUT("update map key(%u) value(%u) err %d", key, val, err);
            return -1;
        }

        return 0;
   }
   ```
   ------
  * 调用fgets时出现段错误，这个问题比较奇怪，至今没找到原因,栈信息如下, 目前怀疑是内存出现越界或覆盖的问题.`已解决`
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
经过几天的时间此问题终于解决，又是因为一个低级错误造成的内存越界,具体是在xdp_framepool.c文件中的xdp_framepool_create中给frame_list分配的空间是sizeof(struct xdp_frame * ),但在访问frame_list的时候是按sizeof(struct xdp_frame * ) * ring_size进行地址访问的，所以造成了内存越界，被覆盖的内存正好是fgets中被使用了，所以造成在调用fgets时出现段错误

------
  * XDP_USE_NEED_WAKEUP标记没起作用.`已解决`
    通过分析内核中ixgbe驱动的源码（开发环境内核5.9.1，测试网卡ixgbe）</br>
    收包流程大致为ixgbe_poll -> ixgbe_clean_rx_irq_zc</br>
    ixgbe_clean_rx_irq_zc中相关代码片段如下(省略其他不相关的代码)</br>
    涉及文件：</br>
    drivers/net/ethernet/intel/ixgbe/ixgbe_main.c</br>
    drivers/net/ethernet/intel/ixgbe/ixgbe_xsk.c</br>
    代码：</br>
    ```
    if (cleaned_count >= IXGBE_RX_BUFFER_WRITE) {
        failure = failure ||
                  !ixgbe_alloc_rx_buffers_zc(rx_ring,
                                 cleaned_count);
            cleaned_count = 0;
        }
     ..........
     if (xsk_umem_uses_need_wakeup(rx_ring->xsk_umem)) {
        if (failure || rx_ring->next_to_clean == rx_ring->next_to_use)
            xsk_set_rx_need_wakeup(rx_ring->xsk_umem);
        else
            xsk_clear_rx_need_wakeup(rx_ring->xsk_umem);

        return (int)total_rx_packets;
    }
    ```
    得到两个信息
    1. 当分配接收缓存失败时会判断是否设置了rx_ring->xsk_umem中的wakeup标记
    2. 即使在rx_ring->xsk_umem中设置了wakeup标记，当分配接收缓存成功后也会清除
    结论是：在分配接收缓存失败时，wakeup才会有效，而且在下次分配成功后会被清除掉，只有wakeup在内核中被设置，在用户代码中的xsk_ring_prod__needs_wakeup才会返回true</br></br>
  * struct xdp_desc * desc中的字段addr通过调用xsk_umem__extract_addr（）可以确定得到的是在umem中的偏移量，但调用xsk_umem__extract_offset（）得到的这个offset代表是什么意思没有搞清楚</br>
    通过分析内核中网卡驱动的源码得知收报逻辑有如下流程</br>
    ixgbe_poll -> ixgbe_clean_rx_irq_zc -> ixgbe_run_xdp_zc -> xdp_do_redirect -> \__bpf_tx_xdp_map -> \__xsk_map_redirect -> xsk_rcv ->
    \__xsk_rcv_zc -> xp_get_handle</br>
    涉及文件:</br>
    drivers/net/ethernet/intel/ixgbe/ixgbe_main.c</br>
    drivers/net/ethernet/intel/ixgbe/ixgbe_xsk.c</br>
    net/core/filter.c</br>
    net/xdp/xsk.c</br>
    xp_get_handle代码</br>
    ```
    static u64 xp_get_handle(struct xdp_buff_xsk *xskb)
    {
      u64 offset = xskb->xdp.data - xskb->xdp.data_hard_start;

      offset += xskb->pool->headroom;
      if (!xskb->pool->unaligned)
          return xskb->orig_addr + offset;
      return xskb->orig_addr + (offset << XSK_UNALIGNED_BUF_OFFSET_SHIFT);
    }
    ```
    结论：offset为数据包内存空间的起始地址减去实际数据的起始地址再加上headroom。</br>
    实际数据包实际的起始地址是指有效的数据开始，</br>
    数据包内存空间的起始地址是指用来存储数据包的内存空间首地址，</br>
    这两个的地址是不同的，留出这段空间是为了方便实现类似ip隧道之类的封装和解封装使用的。</br>
    至于headroom则是用户空间代码中设置的偏移量。
 ------
