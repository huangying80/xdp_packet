# 头文件: xdp_runtime.h
##  结构：
####  struct xdp_runtime<br>
#####  说明：
&emsp;&emsp;xdp_packet运行环境结构，保存xdp_packet所需要的信息

##  定义:
####  XDP_RUNTIME_WORKER_ID 
######  说明：
&emsp;&emsp;获取当前worker的worker_id

####  typedef int (\*xdp_worker_func_t) (volatile void \*)
######  说明:
&emsp;&emsp;用户定义的工作线程


##  函数：
#### int xdp_runtime_init(struct xdp_runtime \*runtime, const char \*ifname, const char \*prog, const char \*sec) 
###### 功能：
&emsp;&emsp;初始化xdp_packet运行环境
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|ifname|网卡设备名|
|prog|加载到网卡的ebpf的字节码程序|
|sec|加载的ebpf字节码中的断|
###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
####  void xdp_runtime_release(struct xdp_runtime \*runtime)
###### 功能：
&emsp;&emsp;释放xdp_packet运行时环境实例
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|

###### 返回值：
&emsp;&emsp;无

----
####  int xdp_runtime_setup_queue(struct xdp_runtime \*runtime, size_t queue_count, size_t queue_size)
###### 功能：
&emsp;&emsp;设置队列
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|queue_count|要使用的队列数|
|queue_size|每个队列的描述符数量|
###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_setup_workers(struct xdp_runtime \*runtime, xdp_worker_func_t worker_func, unsigned short worker_count)
###### 功能：
&emsp;&emsp;设置工作线程
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|worker_func|由用户提供的例程，用于处理收发包逻辑|
|worker_count|工作线程的数量，如果设置成0则与队列数相同并且一一对应|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_startup_workers(struct xdp_runtime \*runtime)
###### 功能：
&emsp;&emsp;启动工作线程
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_setup_numa(struct xdp_runtime \*runtime, int numa_node)
###### 功能：
&emsp;&emsp;设置使用的numa节点
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|numa_node|numa节点|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_setup_rss_ipv4(struct xdp_runtime \*runtime, int protocol)
###### 功能：
&emsp;&emsp;设置网卡多队列的ipv4的哈希key
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|protocol|IPPROTO_TCP或IPPROTO_UDP|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_setup_rss_ipv6(struct xdp_runtime \*runtime, int protocol)
###### 功能：
&emsp;&emsp;设置网卡多队列的ipv6的哈希key
###### 参数：
|参数名|说明|
|------|------|
|runtime|运行时环境实例|
|protocol|IPPROTO_TCP或IPPROTO_UDP|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_tcp_packet(uint16_t port)
###### 功能：
&emsp;&emsp;设置目标端口为参数port的tcp数据包绕过内核协议栈直接传递到用户空间
###### 参数：
|参数名|说明|
|------|------|
|port|端口号|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_tcp_drop(uint16_t port)
###### 功能：
&emsp;&emsp;设置目标端口为参数port的tcp数据包直接被丢掉
###### 参数：
|参数名|说明|
|------|------|
|port|端口号|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_udp_packet(uint16_t port);int xdp_runtime_udp_drop(uint16_t port)
###### 功能：
&emsp;&emsp;设置目标端口为参数port的udp数据包绕过内核协议栈直接传递到用户空间
###### 参数：
|参数名|说明|
|------|------|
|port|端口号|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_l3_packet(uint16_t l3_protocol)
###### 功能：
&emsp;&emsp;设置三层协议号为参数protocol的数据包绕过内核协议栈直接传递到用户空间
###### 参数：
|参数名|说明|
|------|------|
|protocol|三层协议号，目前只支持ipv4和ipv6|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_l3_drop(uint16_t l3_protocal);
###### 功能：
&emsp;&emsp;设置三层协议号为参数protocol的数据包直接被丢掉
###### 参数：
|参数名|说明|
|------|------|
|protocol|三层协议号，目前只支持ipv4和ipv6|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_l4_packet(uint16_t l4_protocol)
###### 功能：
&emsp;&emsp;设置4层协议号为参数protocol的数据包绕过内核协议栈直接传递到用户空间
###### 参数：
|参数名|说明|
|------|------|
|protocol|4层协议号|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_l4_drop(uint16_t l4_protocol)
###### 功能：
&emsp;&emsp;设置4层协议号为参数protocol的数据包直接被丢掉
###### 参数：
|参数名|说明|
|------|------|
|protocol|4层协议号|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_ipv4_packet(const char \*ip, uint32_t prefix, int type)
###### 功能：
&emsp;&emsp;设置与参数ip/prefix匹配的数据包绕过内核协议栈直接传递到用户空间,用于ipv4
###### 参数：
|参数名|说明|
|------|------|
|ip|子网地址|
|prefix|掩码位数|
|type|XDP_UPDATE_IP_SRC: 匹配源地址<br>XDP_UPDATE_IP_DST：匹配目的地址|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_ipv4_drop(const char \*ip, uint32_t prefix, int type);
###### 功能：
&emsp;&emsp;设置与参数ip/prefix匹配的数据包直接丢弃,用于ipv4
###### 参数：
|参数名|说明|
|------|------|
|ip|子网地址|
|prefix|掩码位数|
|type|XDP_UPDATE_IP_SRC: 匹配源地址<br>XDP_UPDATE_IP_DST：匹配目的地址|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_ipv6_packet(const char \*ip, uint32_t prefix, int type)
###### 功能：
&emsp;&emsp;设置与参数ip/prefix匹配的数据包绕过内核协议栈直接传递到用户空间,用于ipv6
###### 参数：
|参数名|说明|
|------|------|
|ip|子网地址|
|prefix|掩码位数|
|type|XDP_UPDATE_IP_SRC: 匹配源地址<br>XDP_UPDATE_IP_DST：匹配目的地址|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
### int xdp_runtime_ipv6_drop(const char \*ip, uint32_t prefix, int type)
###### 功能：
&emsp;&emsp;设置与参数ip/prefix匹配的数据包直接丢弃,用于ipv6
###### 参数：
|参数名|说明|
|------|------|
|ip|子网地址|
|prefix|掩码位数|
|type|XDP_UPDATE_IP_SRC: 匹配源地址<br>XDP_UPDATE_IP_DST：匹配目的地址|

###### 返回值：
|值|说明|
|---|---|
|0|成功|
|-1|失败|

----
# 头文件: xdp_dev.h
### int xdp_dev_read(uint16_t rxq_id, struct xdp_frame \*\*bufs, uint16_t bufs_count)
###### 功能：
&emsp;&emsp;从指定的队列号接收最多bufs_count个数据包放入bufs中
###### 参数：
|参数名|说明|
|------|------|
|rxq_id|接收队列号|
|bufs|数据帧的指针数组|
|bufs_count|bufs的长度|

###### 返回值：
|值|说明|
|---|---|
|\>0|实际接收到的包数量|
|-1|失败|

----
### int xdp_dev_write(uint16_t txq_id, struct xdp_frame \*\*bufs, uint16_t bufs_count)
###### 功能：
&emsp;&emsp;从指定的队列号发送最多bufs_count个数据包
###### 参数：
|参数名|说明|
|------|------|
|txq_id|发送队列号|
|bufs|数据帧的指针数组|
|bufs_count|bufs的长度|

###### 返回值：
|值|说明|
|---|---|
|\>0|实际接收到的包数量|
|-1|失败|

----
### unsigned int xdp_dev_get_empty_frame(uint16_t queue_id, struct xdp_frame \*\*bufs, uint16_t bufs_count)
###### 功能：
&emsp;&emsp;从指定的队列中的frame池中获得bufs_count个空frame并保存在bufs中
###### 参数：
|参数名|说明|
|------|------|
|queue_id|队列号|
|bufs|数据帧的指针数组|
|bufs_count|bufs的长度|

###### 返回值：
|值|说明|
|---|---|
|\>0|获取到的包数量|
|0|获取失败|

----

# 头文件: xdp_framepool.h
##  结构：
####  struct xdp_frame<br>
#####  说明：
&emsp;&emsp;用于描述和存储数据包
|字段|作用|
|----|----|
|data_off|真正的数据存放位置在frame中的偏移|
|data_len|数据字节数|

##  定义:
####  xdp_frame_get_addr(p, t)
######  说明：
&emsp;&emsp;返回frame中指定数据的地址
|参数|说明|
|----|----|
|p|frame指针|
|t|需要转换的数据类型|

##  函数:
####  void xdp_framepool_free_frame(struct xdp_frame \*frame)
###### 功能：
&emsp;&emsp;释放frame,归还到frame池中
###### 参数：
|参数名|说明|
|------|------|
|frame|要释放的frame|


###### 返回值：
&emsp;&emsp;无

----
# 头文件: xdp_net.h
##  函数:
####  static inline uint16_t xdp_ipv4_checksum(const struct iphdr \*ipv4_hdr)
###### 功能：
&emsp;&emsp;计算ipv4效验和
###### 参数：
|参数名|说明|
|------|------|
|ipv4_hdr|ipv4的头部结构|


###### 返回值：
&emsp;&emsp;ip头的效验和

----
### static inline uint16_t xdp_ipv4_udptcp_checksum(const struct iphdr \*iphdr, const void \*l4_hdr)
###### 功能：
&emsp;&emsp;计算ipv4的4层协议的效验和
###### 参数：
|参数名|说明|
|------|------|
|iphdr|指向ip头的指针|
|l4_hdr|指向4层头的指针

###### 返回值：
&emsp;&emsp;计算ipv4的4层协议的效验和

----
