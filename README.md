# SFCaS(Small File Combine and Search)

本项目实现了基于 FUSE 的用户态文件系统，在合并的大文件中通过文件名查找小文件内容，融合了 sindex 学习索引用于索引查询，并通过 gRPC 实现了简单的分布式应用。

[sindex 论文](https://dl.acm.org/doi/abs/10.1145/3409963.3410496)

[sindex 项目地址](https://ipads.se.sjtu.edu.cn:1312/opensource/xindex/-/tree/sindex)

## Prepare

- 安装 MKL(Math Kernel Library)，同时根据安装的目录和版本修改 `Makefile`

  > 可以通过下载 `l_onemkl_p_2023.2.0.49497_offline.sh` 并执行命令 `sh l_onemkl_p_2023.2.0.49497_offline.sh` 进行安装

  ```makefile
  MKL_INCLUDE_DIR := /opt/intel/oneapi/mkl/2023.2.0/include
  MKL_LIB_DIR := /opt/intel/oneapi/mkl/2023.2.0/lib/intel64
  ```

  可能还需要初始化 MKL 的环境变量：

  ```bash
  source /opt/intel/oneapi/setvars.sh
  ```

- 下载安装 libfuse 库：[libfuse/libfuse: The reference implementation of the Linux FUSE (Filesystem in Userspace) interface (github.com)](https://github.com/libfuse/libfuse)

- gRPC 安装：[Quick start | C++ | gRPC](https://grpc.io/docs/languages/cpp/quickstart/)

- 安装 C++ 下的 `Boost` 库



## 索引文件内容

~~~bash
testDir 目录下的 index 内容
`````````````````````````````````````````
``small file num     (8 bytes)         ``
`````````````````````````````````````````
``needle_size        (4 bytes)         ``
``flags              (1 bytes)         ``      
``offset             (8 bytes)         ``      
``size               (4 bytes)         ``         
``filename           ([1, 255] bytes)  ``           
`````````````````````````````````````````
~~~



## 运行

1. 编译生成所需二进制文件，创建相关测试目录：

	```
	$ make build
	```

2. 生成示例小文件并合并（以创建 10000 个文件为例）：

	```
	$ make create
	$ 10000
	$ make combine
	```

3. 运行主程序，在该工作目录下，将 `testDir` 映射到 `mountDir` 上，并将基于 FUSE 实现的文件系统挂载到 `mountDir`：

	```
	$ make run
	```

4. 新开一个终端进行测试（以查询一个文件为例）：

	```
	$ make test
	Test for:
	one file(0) | multiple test(1) | time test(2) | all test(3):0
	Look up for file id:0
	size: 13 buf: hello from 0
	
	Cost time: 775us
	```

	

