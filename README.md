# SFCaS(Small File Combine and Search)

本项目实现了基于 FUSE 的用户态文件系统，从合并的大文件中通过文件名查找存在的小文件内容，融合了 sindex 学习索引用于索引查询。

[sindex 论文](https://dl.acm.org/doi/abs/10.1145/3409963.3410496)

[sindex 项目地址](https://ipads.se.sjtu.edu.cn:1312/opensource/xindex/-/tree/sindex)

## Prepare

- 安装 MKL(Math Kernel Library)，同时根据安装的目录和版本修改 `Makefile`

	```makefile
	MKL_INCLUDE_DIR := /opt/intel/oneapi/mkl/*/include
	MKL_LIB_DIR := /opt/intel/oneapi/mkl/*/lib/intel64
	```

	可能还需要初始化 MKL 的环境变量：

	```bash
	source /opt/intel/oneapi/setvars.sh
	```

- 下载安装 libfuse 库：[libfuse/libfuse: The reference implementation of the Linux FUSE (Filesystem in Userspace) interface (github.com)](https://github.com/libfuse/libfuse)



## 项目结构

```c
.
├── Makefile
├── README.md
├── include
│   ├── constant.h					// 字符串和常数常量定义
│   ├── helper.h					// 输出封装
│   ├── index.h						// 加载索引和模型调用接口
│   ├── needle.h					// 索引项定义和操作
│   ├── sindex						// 学习索引相关头文件
│   │   ├── sindex.h
│   │   ├── sindex_group.h
│   │   ├── sindex_model.h
│   │   ├── sindex_root.h
│   │   └── sindex_util.h
│   └── strkey.h					// 字符串封装
├── mountDir						// 用户态文件系统挂载目录（对该目录文件操作均通过 fuse）
├── src								// 具体实现
│   ├── aux
│   │   ├── index.cpp
│   │   └── needle.cpp
│   ├── combine						// 小文件合并实现
│   │   └── combineFile.cpp
│   ├── sfcas.cpp					// 用户态文件系统实现
│   └── sindex						// 学习索引相关实现
│       ├── sindex.cpp
│       ├── sindex_group.cpp
│       └── sindex_root.cpp
├── test							// 测试用实现
│   ├── createFile.cpp
│   ├── directCreateFile.cpp
│   └── readFile.cpp
└── testDir							// 映射目录（实际操作的目录）
    ├── bigfile
    └── index
```



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

	

