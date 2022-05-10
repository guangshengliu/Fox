# 1.EDK2编译环境安装

```
# 完整从github上下载edk2
git clone https://github.com/tianocore/edk2.git
cd edk2

# 按照下面打开的文件地址下载子模块
# 将下载子模块放入文件指定的地址下
gedit .gitmodules

# 编译前需要安装的工具
# 安装gcc，python
sudo apt install gcc
sudo apt install python 
# 安装qemu
sudo apt install qemu
# 安装NASM
sudo apt install nasm
# 安装asl code
sudo apt install iasl
# 安装uuid库
sudo apt install uuid-dev
```

# 2.EDK2编译运行

```
# 编译基本工具
source edksetup.sh BaseTools
make -C BaseTools

# 按照默认配置编译Ovmf
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc

# 进入运行目录
cd Build/OvmfX64/DEBUG_GCC5/FV
# 启动运行
qemu-system-x86_64 -bios OVMF.fd
```

# 3.EDK2编译生成shell.efi

```
# 以下为Conf/target.text的配置
ACTIVE_PLATFORM = ShellPkg/ShellPkg.dsc
TOOL_CHAIN_TAG = GCC5
TARGET_ARCH = X64

# 按照默认配置编译Shell.efi
build -a X64 -t GCC5 -p ShellPkg/ShellPkg.dsc
# 编译后会生成2个Shell.efi文件，选择其中一个改成BooTx64.efi
# 挂载U盘
mount /dev/sdb1 /mnt/
# U盘根目录创建/EFI/BOOT
mkdir /mnt/EFI/BOOT
# 将Shell.efi改成BooTx64.efi并复制到/EFI/BOOT目录下
cp /edk2/Build/Shell/DEBUG_GCC5/X64/Shell.efi BOOTx64.EFI
sync
# 取消挂载
umount /mnt/
```

# 4.EDK2编译.c文件生成.efi文件

```
# 从github上下载edk2-libc，编译自己编写的程序
git clone https://github.com/tianocore/edk2-libc.git
# 以下为Conf/target.text的配置
ACTIVE_PLATFORM = AppPkg/AppPkg.dsc
TOOL_CHAIN_TAG = GCC5
TARGET_ARCH = X64
# 编译基本工具
source edksetup.sh BaseTools
make -C BaseTools
# 在AppPkg/AppPkg.dsc的[Components]中添加
AppPkg/Applications/Video/Video.inf
# 编译生成.efi文件
build -a X64 -t GCC5 -p AppPkg/AppPkg.dsc
```
