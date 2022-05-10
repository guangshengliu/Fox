;===================================虚拟软盘创建FAT12文件引导扇区数据
; 指定程序的起始地址
	org	0x7c00
; 将标识符BaseOfStack等价为0x7c00
BaseOfStack	equ	0x7c00
;=========  下面两段代码组合成了Loader程序的起始物理地址
;   地址转化公式BaseOfLoader << 4 + OffsetOfLoader = 0x10000
BaseOfLoader	equ	0x1000
OffsetOfLoader	equ	0x00

; 根目录占用的扇区数
; (BPB_RootEntCnt*32+BPB_BytesPerSec-1)/BPB_BytesPerSec =
; (224 x 32 + 512 - 1) / 512 = 14
RootDirSectors	equ	14

; 根目录的起始扇区号
; 保留扇区数+FAT表扇区数*FAT表份数=1+9*2=19
SectorNumOfRootDirStart	equ	19

; FAT1表的起始扇区号，引导扇区号为0
SectorNumOfFAT1Start	equ	1

; 平衡文件的起始簇号与数据区起始簇号的差值
SectorBalance	equ	17	

	jmp	short Label_Start
	nop
	BS_OEMName	    db	'MINEboot'
	BPB_BytesPerSec	dw	512
	BPB_SecPerClus	db	1
	BPB_RsvdSecCnt	dw	1
	BPB_NumFATs	    db	2
	BPB_RootEntCnt	dw	224
	BPB_TotSec16	dw	2880
	BPB_Media	    db	0xf0
	BPB_FATSz16	    dw	9
	BPB_SecPerTrk	dw	18
	BPB_NumHeads	dw	2
	BPB_HiddSec	    dd	0
	BPB_TotSec32	dd	0
	BS_DrvNum	    db	0
	BS_Reserved1	db	0
	BS_BootSig	    db	0x29
	BS_VolID	    dd	0
	BS_VolLab	    db	'boot loader'
	BS_FileSysType	db	'FAT12   '

Label_Start:
    ; 将cs寄存器的段基地址设置到DS,ES,SS等寄存器中
	mov	ax,	cs
	mov	ds,	ax
	mov	es,	ax
	mov	ss,	ax
	mov	sp,	BaseOfStack  ; 设置栈指针寄存器sp

;==================  清屏功能

	mov	ax,	0600h
	mov	bx,	0700h
	mov	cx,	0
	mov	dx,	0184fh
	int	10h

;==================  设置屏幕光标位置在屏幕左上角(0,0)处

	mov	ax,	0200h
	mov	bx,	0000h
	mov	dx,	0000h
	int	10h

;==================  显示字符串:Start Booting......

	mov	ax,	1301h
	mov	bx,	000fh
	mov	dx,	0000h
	mov	cx,	10
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	StartBootMessage
	int	10h

;=================  软盘驱动器的复位

	xor	ah,	ah
	xor	dl,	dl
	int	13h

;========================= 目标文件搜索功能
	mov	word	[SectorNo],	SectorNumOfRootDirStart

Lable_Search_In_Root_Dir_Begin:

	cmp	word	[RootDirSizeForLoop],	0
	jz	Label_No_LoaderBin
	dec	word	[RootDirSizeForLoop]	
	mov	ax,	00h
	mov	es,	ax
	mov	bx,	8000h
	mov	ax,	[SectorNo]
	mov	cl,	1
	call	Func_ReadOneSector
	mov	si,	LoaderFileName
	mov	di,	8000h
	cld
	mov	dx,	10h
	
Label_Search_For_LoaderBin:

	cmp	dx,	0
	jz	Label_Goto_Next_Sector_In_Root_Dir
	dec	dx
	mov	cx,	11

Label_Cmp_FileName:

	cmp	cx,	0
	jz	Label_FileName_Found
	dec	cx
	lodsb	
	cmp	al,	byte	[es:di]
	jz	Label_Go_On
	jmp	Label_Different

Label_Go_On:
	
	inc	di
	jmp	Label_Cmp_FileName

Label_Different:

	and	di,	0ffe0h
	add	di,	20h
	mov	si,	LoaderFileName
	jmp	Label_Search_For_LoaderBin

Label_Goto_Next_Sector_In_Root_Dir:
	
	add	word	[SectorNo],	1
	jmp	Lable_Search_In_Root_Dir_Begin
	
;=======	display on screen : ERROR:No LOADER Found

Label_No_LoaderBin:

	mov	ax,	1301h
	mov	bx,	008ch
	mov	dx,	0100h
	mov	cx,	21
	push	ax
	mov	ax,	ds
	mov	es,	ax
	pop	ax
	mov	bp,	NoLoaderMessage
	int	10h
	jmp	$

;=============== 从FAT12文件系统加载loader.bin文件到内存的过程

Label_FileName_Found:

	mov	ax,	RootDirSectors
	and	di,	0ffe0h
	add	di,	01ah
	mov	cx,	word	[es:di]
	push	cx
	add	cx,	ax
	add	cx,	SectorBalance
	mov	ax,	BaseOfLoader
	mov	es,	ax
	mov	bx,	OffsetOfLoader
	mov	ax,	cx

Label_Go_On_Loading_File:
	push	ax
	push	bx
	mov	ah,	0eh
	mov	al,	'.'
	mov	bl,	0fh
	int	10h
	pop	bx
	pop	ax

	mov	cl,	1
	call	Func_ReadOneSector
	pop	ax
	call	Func_GetFATEntry
	cmp	ax,	0fffh
	jz	Label_File_Loaded
	push	ax
	mov	dx,	RootDirSectors
	add	ax,	dx
	add	ax,	SectorBalance
	add	bx,	[BPB_BytesPerSec]
	jmp	Label_Go_On_Loading_File

Label_File_Loaded:
	
	jmp	BaseOfLoader:OffsetOfLoader

;===============================虚拟软盘读取功能的程序实现

Func_ReadOneSector:
	; 将逻辑块寻址转化为柱面/磁头/扇区格式的磁盘扇区号
	push	bp
	mov	bp,	sp
	sub	esp,	2
	mov	byte	[bp - 2],	cl
	push	bx
	mov	bl,	[BPB_SecPerTrk]
    div bl      ; 起始扇区号除以每磁道扇区数(BL)
    inc ah      ; 余数保存在ah中，且加1
	mov	cl,	ah
	mov	dh,	al
	shr	al,	1       ; 逻辑左移
	mov	ch,	al
	and	dh,	1
	pop	bx
	mov	dl,	[BS_DrvNum]
Label_Go_On_Reading:
	mov	ah,	2
	mov	al,	byte	[bp - 2]
	int	13h
	jc	Label_Go_On_Reading     ; 条件转移
	add	esp,	2
	pop	bp
	ret

;======================= FAT表项解析,根据当前FAT表项索引出下一个FAT表项

Func_GetFATEntry:

	push	es
	push	bx
	push	ax
	mov	ax,	00
	mov	es,	ax
	pop	ax
	mov	byte	[Odd],	0
	mov	bx,	3
	mul	bx
	mov	bx,	2
	div	bx
	cmp	dx,	0
	jz	Label_Even
	mov	byte	[Odd],	1

Label_Even:

	xor	dx,	dx
	mov	bx,	[BPB_BytesPerSec]
	div	bx
	push	dx
	mov	bx,	8000h
	add	ax,	SectorNumOfFAT1Start
	mov	cl,	2
	call	Func_ReadOneSector
	
	pop	dx
	add	bx,	dx
	mov	ax,	[es:bx]
	cmp	byte	[Odd],	1
	jnz	Label_Even_2
	shr	ax,	4

Label_Even_2:
	and	ax,	0fffh
	pop	bx
	pop	es
	ret

;====================   保存程序运行临时数据

RootDirSizeForLoop	dw	RootDirSectors
SectorNo		dw	0
Odd			db	0

;====================   日志信息

StartBootMessage:	db	"Start Boot"
NoLoaderMessage:	db	"ERROR:No LOADER Found"
LoaderFileName:		db	"LOADER  BIN",0

;==========================================================================
    ; $ - $$的作用是计算出当前程序生成的机器码长度
    ; 引导扇区必须填充的数据长度 510 - ($ - $$)，保证生产文件大小为512B(一个扇区大小)
    times   510 - ($ - $$)  db  0
    ; 引导扇区以0x55和0xaa两个字节结尾，小端存储
    dw      0xaa55

