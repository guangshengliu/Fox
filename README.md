## Ubuntu环境配置

切换root用户

```
su root 
```

gcc	gcc c++安装

```
# 更新包列表
sudo apt update
# 安装build-essential软件包
sudo apt install build-essential
# 查看gcc版本
gcc --version
```

NASM汇编语言编译器安装

```
# 安装NASM
sudo apt-get install nasm
# 查看nasm版本
nasm -version
```

Bochs-2.6.11安装

```
# 下载相关依赖
sudo apt-get install build-essential
sudo apt-get install xorg-dev
sudo apt-get install libgtk2.0-dev

# 下载Bochs-2.6.11
wget https://nchc.dl.sourceforge.net/project/bochs/bochs/2.6.9/bochs-2.6.11.tar.gz
# 解压
tar vxaf bochs-2.6.11.tar.gz

# 进入文件目录
cd bochs-2.6.11
# 读取配置文件
./configure --with-x11 --with-wx --enable-debugger --enable-disasm --enable-all-optimizations --enable-readline --enable-long-phy-address --enable-ltdl-install --enable-idle-hack --enable-plugins --enable-a20-pin --enable-x86-64 --enable-smp --enable-cpu-level=6 --enable-large-ramfile --enable-repeat-speedups --enable-fast-function-calls  --enable-handlers-chaining  --enable-trace-linking --enable-configurable-msrs --enable-show-ips --enable-cpp --enable-debugger-gui --enable-iodebug --enable-logging --enable-assert-checks --enable-fpu --enable-vmx=2 --enable-svm --enable-3dnow --enable-alignment-check  --enable-monitor-mwait --enable-avx  --enable-evex --enable-x86-debugger --enable-pci --enable-usb --enable-voodoo
# 编译前准备，否则会出错误
cp misc/bximage.cpp misc/bximage.cc
cp iodev/hdimage/hdimage.cpp iodev/hdimage/hdimage.cc
cp iodev/hdimage/vmware3.cpp iodev/hdimage/vmware3.cc
cp iodev/hdimage/vmware4.cpp iodev/hdimage/vmware4.cc
cp iodev/hdimage/vpc-img.cpp iodev/hdimage/vpc-img.cc
cp iodev/hdimage/vbox.cpp iodev/hdimage/vbox.cc
# 编译
make 
# 安装
sudo make install 
# 配置文件
cp .bochsrc bochsrc
```

安装问题：In function void bx_dbg_tlb_lookup(bx_lin_address)

在bx_debug/dbg_main.cc中找到下面的定义，修改成：

```
Bit32u index = BX_ITLB_INDEX_OF(laddr);		//这一行改成下面一行
Bit32u index = BX_CPU(dbg_cpu)->ITLB.get_index_of(laddr);

sprintf(cpu_param_name, "ITLB.entry%d", index);
bx_dbg_show_param_command(cpu_param_name, 0);

index = BX_DTLB_INDEX_OF(laddr, 0);		//这一行改成下面一行
index = BX_CPU(dbg_cpu)->DTLB.get_index_of(laddr);
```



## git配置

```
# 安装git
sudo apt-get install git

# 配置git
git config --global user.name "github的昵称"
git config --global user.email "注册github的邮箱"
# 生成链接公钥
ssh-keygen -C '注册github的邮箱' -t rsa

cd ~/.ssh
# 打开密钥文件
gedit id_rsa.pub
```

接着访问项目所在网页，点击SHH公钥，标题栏可以随意输入，公钥栏把你刚才复制的内容粘贴进去就OK了。

接下来进行文件上传

```
# 从远端复制
git clone git@github.com:guangshengliu/Dreamux.git
# 仓库初始化
git init
# 添加文件
git add .
# 提交
git commit -m "注释语句"
# 设置源仓库
git remote add origin git@github.com:guangshengliu/Dreamux.git
# 配置错误时更改源仓库
git remote rm origin
# push内容
git push -u origin master
```



## Dreamux操作系统相关文件生成

编译NASM得到boot.bin

```
nasm src/boot/boot.asm -o boot.bin
```

创建一个dreamux.img

```
bximage
1
fd	# 选择软盘
1.44MB	# 大小选择
dreamux.img	# 名字命名
```

将boot.bin写入dreamux.img

```
dd if=src/boot/boot.bin of=dreamux.img bs=512 count=1 conv=notrunc
```

打开bochs虚拟机

```
bochs -f bochsrc
```

进入loader.asm所在目录

编译NASM得到loader.bin

```
nasm loader.asm -o loader.bin
```

进入src/kernel目录下

```
# 根据makefile文件编译直接得到kernel.bin
make
# 清除make文件，重新make
make clean
```

挂载dreamux.img

```
# 查看第一个空闲loop设备 
sudo losetup -f
# 查看所以忙碌loop设备
losetup -a
# 结果： /dev/loop0

# 设置回环设备
sudo losetup /dev/loop0 dreamux.img

# 挂载dreamux/mdeia/
mount /dev/loop0 /media/
# 引导loader.bin负责到文件系统
cp src/boot/loader.bin /media/
# 引导kernel.bin负责到文件系统
cp src/kernel/kernel.bin /media/
sync
# 解除挂载
umount /media/

# 删除回环设备
sudo losetup -d /dev/loop0
```

一键复制粘贴操作

```
sudo losetup /dev/loop0 dreamux.img
mount /dev/loop0 /media/
cp src/kernel/kernel.bin /media/
sync
umount /media/
sudo losetup -d /dev/loop0
bochs -f bochsrc
```

gcc版本切换

```
# 50表示优先级
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 50
# 40表示优先级
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 40

sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 50 

sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 40
# 切换gcc版本
sudo update-alternatives --config gcc
sudo update-alternatives --config g++
```



