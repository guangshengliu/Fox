#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>

// 图形显示信息
struct EFI_GRAPHICS_OUTPUT_INFORMATION
{
    unsigned int HorizontalResolution;
    unsigned int VerticalResolution;
    unsigned int PixelsPerScanLine;
    unsigned long FrameBufferBase;
    unsigned long FrameBufferSize;
};
// 内存描述符
struct EFI_E820_MEMORY_DESCRIPTOR
{
    unsigned long address;
    unsigned long length;
    unsigned int type;
}__attribute__((packed));
// 内存描述符信息
struct EFI_E820_MEMORY_DESCRIPTOR_INFORMATION
{
    unsigned int E820_Entry_count;
    struct EFI_E820_MEMORY_DESCRIPTOR E820_Entry[0];
};
// 内核参数信息
struct KERNEL_BOOT_PARAMETER_INFORMATION
{
    struct EFI_GRAPHICS_OUTPUT_INFORMATION Graphics_Info;
    struct EFI_E820_MEMORY_DESCRIPTOR_INFORMATION E820_Info;
};

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE *SystemTable)
{

    EFI_LOADED_IMAGE        *LoadedImage;
    EFI_FILE_IO_INTERFACE   *Vol;                   //  根目录所在卷
    EFI_FILE_HANDLE         RootFs;                 //  根目录的句柄
    EFI_FILE_HANDLE         FileHandle;             //  将要打开文件的句柄
    int i = 0;
    void (*func)(void);
    EFI_STATUS status = EFI_SUCCESS;
    struct KERNEL_BOOT_PARAMETER_INFORMATION *kernel_boot_para_info = NULL;
    // 检索ImageHandle句柄
    gBS->HandleProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,(VOID**)&LoadedImage);
    // 进一步检索LoadedImage->DeviceHandle句柄
    gBS->HandleProtocol(LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(VOID**)&Vol);
    // 获取分区的句柄RootFs
    Vol->OpenVolume(Vol,&RootFs);
    // 打开kernel.bin文件
    status = RootFs->Open(RootFs,&FileHandle,(CHAR16*)L"kernel.bin",EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(status))
    {
        Print(L"Open kernel.bin Failed.\n");
        return status;
    }
    EFI_FILE_INFO* FileInfo;        //  目标文件信息
    UINTN BufferSize = 0;           //  目标文件大小
    EFI_PHYSICAL_ADDRESS pages = 0x100000;
    BufferSize = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 100;
    // 分配长度为BufferSize大小的内存，内存类型为运行时服务程序的数据区
    // FileInfo指向分配的内存空间，该空间用于存储内存描述符结构数组
    gBS->AllocatePool(EfiRuntimeServicesData,BufferSize,(VOID**)&FileInfo);
    // 获取kernel.bin相关信息
    FileHandle->GetInfo(FileHandle,&gEfiFileInfoGuid,&BufferSize,FileInfo);
    Print(L"\tFileName:%s\t Size:%d\t FileSize:%d\t PhysicalSize:%d\n",FileInfo->FileName,FileInfo->Size,FileInfo->FileSize,FileInfo->PhysicalSize);
    // 从0x100000地址处分配目标文件需占的物理页数
    gBS->AllocatePages(AllocateAddress,EfiConventionalMemory,(FileInfo->FileSize + 0x1000 - 1) / 0x1000,&pages);
    Print(L"Read Kernel File to Memory Address:%018lx\n",pages);
    BufferSize = FileInfo->FileSize;
    // 读取kernel.bin文件保存的数据到0x100000
    FileHandle->Read(FileHandle,&BufferSize,(VOID*)pages);
    gBS->FreePool(FileInfo);
    FileHandle->Close(FileHandle);
    RootFs->Close(RootFs);
    /////////////////////////////////////////////////////////////////////////
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gGraphicsOutput = 0;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info = 0;
    UINTN InfoSize = 0;
    pages = 0x60000;
    kernel_boot_para_info = (struct KERNEL_BOOT_PARAMETER_INFORMATION *)0x60000;
    // 在物理地址0x60000处为KERNEL_BOOT_PARAMETER_INFORMATION结构体分配一个内存页面
    gBS->AllocatePages(AllocateAddress,EfiConventionalMemory,1,&pages);
    // 将刚刚分配的内存全部置为0
    gBS->SetMem((void*)kernel_boot_para_info,0x1000,0);
    gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID**)&gGraphicsOutput);
    long H_V_Resolution = gGraphicsOutput->Mode->Info->HorizontalResolution * gGraphicsOutput->Mode->Info->VerticalResolution;
    int MaxResolutionMode = gGraphicsOutput->Mode->Mode;
    // 遍历所有模式，找到最大的分辨率
    for(i = 0;i < gGraphicsOutput->Mode->MaxMode;i++){
        gGraphicsOutput->QueryMode(gGraphicsOutput,i,&InfoSize,&Info);
        if((Info->PixelFormat == 1) && (Info->HorizontalResolution* Info->VerticalResolution > H_V_Resolution))
        {
            H_V_Resolution = Info->HorizontalResolution * Info->VerticalResolution;
            MaxResolutionMode = i;
        }
        gBS->FreePool(Info);
    }
    // 设置图形设备的显示信息，并清除显示区域的内容
    gGraphicsOutput->SetMode(gGraphicsOutput,MaxResolutionMode);
    gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID**)&gGraphicsOutput);
    Print(L"CurrentMode:%02d,Version:%x,Format:%d,Horizontal:%d,Vertical:%d,ScanLine:%d,FrameBufferBase:%018lx,FrameBufferSize:%018lx\n",gGraphicsOutput->Mode->Mode,gGraphicsOutput->Mode->Info->Version,gGraphicsOutput->Mode->Info->PixelFormat,gGraphicsOutput->Mode->Info->HorizontalResolution,gGraphicsOutput->Mode->Info->VerticalResolution,gGraphicsOutput->Mode->Info->PixelsPerScanLine,gGraphicsOutput->Mode->FrameBufferBase,gGraphicsOutput->Mode->FrameBufferSize);
    // 用KERNEL_BOOT_PARAMETER_INFORMATION结构体保存当前图形设备的显示信息
    kernel_boot_para_info->Graphics_Info.HorizontalResolution = gGraphicsOutput->Mode->Info->HorizontalResolution;
    kernel_boot_para_info->Graphics_Info.VerticalResolution = gGraphicsOutput->Mode->Info->VerticalResolution;
    kernel_boot_para_info->Graphics_Info.PixelsPerScanLine = gGraphicsOutput->Mode->Info->PixelsPerScanLine;
    kernel_boot_para_info->Graphics_Info.FrameBufferBase = gGraphicsOutput->Mode->FrameBufferBase;
    kernel_boot_para_info->Graphics_Info.FrameBufferSize = gGraphicsOutput->Mode->FrameBufferSize;
    Print(L"Map Graphics FrameBufferBase to Virtual Address 0xffff800003000000\n");
    long * PageTableEntry = (long *)0x103000;
    // map to virtual address 0xffff800003000000
    for(i = 0;i < (gGraphicsOutput->Mode->FrameBufferSize + 0x200000 - 1) >> 21;i++)
    {
        *(PageTableEntry + 24 + i) = gGraphicsOutput->Mode->FrameBufferBase | 0x200000 * i | 0x87;
        Print(L"Page %02d,Address:%018lx,Value:%018lx\n",i,(long)(PageTableEntry + 24 + i),*(PageTableEntry + 24 + i));
    }
    //////////////////////////////////////////////////////////
    // 根据地址空间类型转换为E820内存结构
    struct EFI_E820_MEMORY_DESCRIPTOR *E820p = kernel_boot_para_info->E820_Info.E820_Entry;
    struct EFI_E820_MEMORY_DESCRIPTOR *LastE820 = NULL;
    unsigned long LastEndAddr = 0;
    int E820Count = 0;
    UINTN MemMapSize = 0;                   // 内存描述符结构数组存储空间的长度
    EFI_MEMORY_DESCRIPTOR* MemMap = 0;      // 内存描述符结构数组存储空间
    UINTN MapKey = 0;                       // 当前内存映射键值
    UINTN DescriptorSize = 0;               // 内存描述符结构的长度
    UINT32 DesVersion = 0;                  // 内存描述符结构的版本号
    // 返回当前物理地址空间信息
    gBS->GetMemoryMap(&MemMapSize,MemMap,&MapKey,&DescriptorSize,&DesVersion);
    MemMapSize += DescriptorSize * 5;
    // 分配长度为MemMapSize大小的内存，内存类型为运行时服务程序的数据区
    // MemMap指向分配的内存空间，该空间用于存储内存描述符结构数组
    gBS->AllocatePool(EfiRuntimeServicesData,MemMapSize,(VOID**)&MemMap);
    Print(L"GetMemMapSize:%d,DescriptorSize:%d,count:%d\n",MemMapSize,DescriptorSize,MemMapSize/DescriptorSize);
    // 将分配的空间全置为0
    gBS->SetMem((void*)MemMap,MemMapSize,0);
    // 返回当前物理地址空间信息，确保前后2次一致
    status = gBS->GetMemoryMap(&MemMapSize,MemMap,&MapKey,&DescriptorSize,&DesVersion);
    Print(L"GetMemMapSize:%d,DescriptorSize:%d,count:%d\n",MemMapSize,DescriptorSize,MemMapSize/DescriptorSize);
    if(EFI_ERROR(status))
        Print(L"status:%018lx\n",status);
    Print(L"Get EFI_MEMORY_DESCRIPTOR Structure:%018lx\n",MemMap);
    // 根据地址空间类型转换为E820内存结构
    for(i = 0;i < MemMapSize / DescriptorSize;i++)
    {
        int MemType = 0;
        EFI_MEMORY_DESCRIPTOR* MMap = (EFI_MEMORY_DESCRIPTOR*)((CHAR8*)MemMap + i * DescriptorSize);
        if(MMap->NumberOfPages == 0)
            continue;
        Print(L"MemoryMap %4d %10d (%16lx<->%16lx)%016lx\n",MMap->Type,MMap->NumberOfPages,MMap->PhysicalStart,MMap->PhysicalStart + (MMap->NumberOfPages << 12),MMap->Attribute);
        switch(MMap->Type)
        {
        case EfiReservedMemoryType:
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
        MemType= 2;
        //2:ROM or Reserved
        break;
        case EfiUnusableMemory:
        MemType= 5;
        //5:Unusable
        break;
        case EfiACPIReclaimMemory:
        MemType= 3;
        //3:ACPI Reclaim Memory
        break;
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiConventionalMemory:
        case EfiPersistentMemory:
            MemType= 1;
            //1:RAM
            break;
        case EfiACPIMemoryNVS:
            MemType= 4;
            //4:ACPI NVS Memorybreak;
        default:
            Print(L"Invalid UEFI Memory Type:%4d\n",MMap->Type);
            continue;
        }
        if((LastE820 != NULL) && (LastE820->type == MemType) && (MMap->PhysicalStart == LastEndAddr))
        {
            LastE820->length += MMap->NumberOfPages << 12;
            LastEndAddr += MMap->NumberOfPages << 12;
        }
        else
        {
        E820p->address = MMap->PhysicalStart;
        E820p->length = MMap->NumberOfPages << 12;
        E820p->type = MemType;
        LastEndAddr = MMap->PhysicalStart + (MMap->NumberOfPages << 12);
        LastE820 = E820p;
        E820p++;
        E820Count++;
        }
    }
    kernel_boot_para_info->E820_Info.E820_Entry_count = E820Count;
    LastE820 = kernel_boot_para_info->E820_Info.E820_Entry;
    int j = 0;
    for(i = 0; i< E820Count; i++)
    {
        struct EFI_E820_MEMORY_DESCRIPTOR* e820i = LastE820 + i;
        struct EFI_E820_MEMORY_DESCRIPTOR MemMap;
        for(j = i + 1; j< E820Count; j++)
        {
            struct EFI_E820_MEMORY_DESCRIPTOR* e820j = LastE820 + j;
            if(e820i->address > e820j->address)
            {
                MemMap = *e820i;
                *e820i = *e820j;
                *e820j = MemMap;
            }
        }
    }
    // 将转换后的信息保存到KERNEL_BOOT_PARAMETER_INFORMATION结构
    LastE820 = kernel_boot_para_info->E820_Info.E820_Entry;
    for(i = 0;i < E820Count;i++)
    {
        Print(L"MemoryMap (%10lx<->%10lx) %4d\n",LastE820->address,LastE820->address+LastE820->length,LastE820->type);
        LastE820++;
    }
    gBS->FreePool(MemMap);
    Print(L"Call ExitBootServices And Jmp to Kernel.\n");
    gBS->GetMemoryMap(&MemMapSize,MemMap,&MapKey,&DescriptorSize,&DesVersion);
    gBS->CloseProtocol(LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,ImageHandle,NULL);
    gBS->CloseProtocol(ImageHandle,&gEfiLoadedImageProtocolGuid,ImageHandle,NULL);
    gBS->CloseProtocol(gGraphicsOutput,&gEfiGraphicsOutputProtocolGuid,ImageHandle,NULL);
    status = gBS->ExitBootServices(ImageHandle,MapKey);
    if(EFI_ERROR(status))
    {
        Print(L"ExitBootServices: Failed, Memory Map has Changed.\n");
        return EFI_INVALID_PARAMETER;
    }
    func = (void *)0x100000;
    func();
    return EFI_SUCCESS;
}
