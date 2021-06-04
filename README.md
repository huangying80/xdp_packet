**xdp_packet**<br>
huangying, email: hy_gzr@163.com
====

## xdp_packet简介
   * 基于xdp及ebpf开发,不存在像dpdk一样用uio驱动接管内核网卡驱动造成的网络接口不可见的问题。
   * 可根据配置规则决定网络包传递给内核协议栈，或丢弃，或直接传递到用户空间。
   * 高性能网络收包框架即网络数据平面开发框架，单CPU核单网卡队列的环境下，包转发性能可达百万PPS。并且在增加CPU核心数及网卡队列数且numa本地访问时，性能满足线性增长。目前可用于DNS服务、负载均衡，三四层路由等应用。
   * 同时支持高性能的丢包处理，单CPU核单网卡队列的环境下，丢包可达9百万pps。目前支持规则，源IP及子网，目标IP及子网，源端口，目标端口，目前可用于防火墙等应用.
   *  支持使用大页内存，以提高性能。
   *  对内核版本、内核网卡驱动、网卡型号有要求，详细支持情况请参考<br>
      https://github.com/iovisor/bcc/blob/master/docs/kernel-versions.md#xdp
### 本项目用于交流和学习，请勿用于非正当途径

因为dpdk需要用uio驱动接管网卡，当应用出现问题时，维护起来比较麻烦，可能需要两块网卡，一块用于收发包处理，一个用于远程登陆 但是xdp可以将ebpf程序加载到网卡驱动中执行，而不需要从内核中将网卡接管。利用此特性给维护提供了方便，不需要额外的管理网卡就可以远程登录。 dpdk中虽然也支持af_xdp,但是低版本中存在个别的bug,在一定条件下会触发段错误，在低版本中会存在性能不够的情况（这些问题，dpdk官方已经有补丁） 又因为dpdk强大的功能导致整个源码复杂庞大，不太方便学习，所以决定搞一个简单点的，基于xdp的高性能收发包框架xdp_packet,以便于学习和应用到之后 的工作中，此项目需要了解xdp,ebpf为基础

此开源项目利用XDP中的af_xdp实现了网络包绕过了内核协议栈直接到用户空间的处理，且不需要像dpdk那样，需要用uio驱动接管内核的网卡驱动 此项目目前刚刚开始，正处于开发阶段，编译完成，但还没有经过任何调试，所以会存在许多bug.

同时非常感谢dpdk提供的支持和参考以及宝贵的系统经验！

#### 编译:
* 依赖：至少满足以下版本
  * centos 7.6.1810
  * kerne 5.9.1 内核需要开启特定配置选项以支持xdp 请参考https://docs.cilium.io/en/v1.8/bpf/#compiling-the-kernel
  * gcc/g++ version 8.3.1
  * LLVM version 9.0.1
  * clang version 9.0.1
  * libpthread-2.17.so
  * elfutils-libelf-devel-0.176-5
  * numactl-libs-2.0.12-5
  * libz.so.1.2.7
* 下载
  * git clone https://github.com/huangying80/xdp_packet.git
* 编译：生成静态库libxdppacket.a和ebpf程序xdp_kern_prog/xdp_kern_prog.o
  * cd xdp_packet
  * make
* 示例
  * https://github.com/huangying80/xdp_packet/tree/main/sample
  
#### 使用手册：
https://github.com/huangying80/xdp_packet/blob/main/manual.md
#### 开发中遇到的一些问题及解决办法：
https://github.com/huangying80/xdp_packet/blob/main/Note.md<br>

#### 修改历史：
  **已经移动到Changelog.md**<br>
  https://github.com/huangying80/xdp_packet/blob/main/Changelog.md<br>

#### 开发进度：
  * 完善接口说明文档 `进行中`
  * 单核单网卡队列收发包功能调试 `已完成`
  * 大页内存功能调试 `已完成`
  * 调试在多核多网卡队列下的收发包处理 `已完成`
  * 单核单网卡队列下的测试程序sample/dns_server的压力测试 `已完成`
  * 单核单网卡队列下且设置numa节点的测试程序sample/dns_server的压力测试 `已完成`
  * 多核多网卡队列下的测试程序sample/dns_server的压力测试 `已完成`
  * 多核多网卡队列下且设置numa节点的测试程序sample/dns_server的压力测试 `已完成`

 
