# Linux 文件传输：双虚拟机初版

这是作为 Git 首次提交基线的版本，保留已经完成双虚拟机试验所需的 INADDR_ANY 监听设置。

## 当前功能

- IPv4 TCP 文件上传，每个连接上传一个文件，服务器依次处理连接。
- 协议为 4 字节文件长度（网络字节序）加文件内容。
- 最大文件大小 64 MiB，使用 4096 字节缓冲区分块收发，允许空文件。
- 服务器先写临时文件，完成后保存为 received.bin，再回复一个字节：0 成功，1 失败。
- 本版不传输原始文件名，下一次成功上传会替换 received.bin。
- 收发失败时关闭资源、清理临时文件；强制退出可能留下临时文件。

扩展版的文件名传输、分阶段审核、拒绝同名覆盖和自动测试暂未包含。等初版提交到 GitHub 后再添加，以便通过 Git 查看差异。

## 工程文件

- common.h：公共常量与函数声明。
- common.c：send_all、recv_exact 和 parse_port 实现。
- file_client.c：读取文件、连接服务器、发送长度与内容、接收结果。
- file_server.c：监听连接、接收文件、保存并回复结果。
- CMakeLists.txt：编译配置。
- .gitignore：忽略构建目录、接收文件与本地测试数据。

## 编译

在 Linux 工程目录中执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 双虚拟机运行

沿用已经验证互通的 NAT 网络。示例服务器为 192.168.33.137，客户端为 192.168.33.138；实际地址变化时相应替换。

服务器虚拟机：

```bash
cd ~/linux-file-transfer
mkdir -p uploads
./build/file_server --port 9000 --dir ./uploads
```

客户端虚拟机：

```bash
cd ~/linux-file-transfer
printf 'Hello Linux!\n' > example.txt
./build/file_client --host 192.168.33.137 --port 9000 --file ./example.txt
sha256sum example.txt
```

服务器另开终端：

```bash
cd ~/linux-file-transfer
sha256sum uploads/received.bin
```

比较两个哈希值确认内容一致。按 Ctrl+C 停止服务器。本机测试可将客户端目标地址改为 127.0.0.1。

## 当前限制

本版没有超时、并发处理、认证、加密和断点续传。成功回复不提供断电持久性保证；如果最终回复丢失，服务器也可能已保存文件而客户端报告失败。上传期间请勿修改源文件。

当前还原已检查源码和协议逻辑；未在本机 Windows 环境重新执行 Linux 编译和运行测试。
