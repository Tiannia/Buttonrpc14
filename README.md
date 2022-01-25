原仓库地址：[buttonrpc_cpp14](https://github.com/Tiannia/buttonrpc_cpp14)

#### 改动:
- 在linux平台上使用cmake构建项目；
- [ZMQ](https://zeromq.org/download/)使用apt-get下载:`apt-get install libzmq3-dev`，在cmake链接相应库即可；
- 经测试，能够完成linux平台与windows平台的rpc通信；
- 添加相应注释。

#### TODO:
- 序列化机制需要完善，减少数据拷贝次数，内存保护等；
- 实现多客户端连接传输（并发）；
- ......