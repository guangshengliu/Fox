# How to run it ?

1. A USB flash disk supporting the installation system (with a space size of more than 100 MB)
2. Copy the files in the UEFI directory to the root directory of the USB flash disk
3. Start your computer, go to the startup item selection page (Lenovo laptops normally press and hold F12), select USB flash disk boot
4. Wait for the system to load

# Version 1.0.0
1. Version 1.0.0 ports operating systems from virtual machines to physical machines and currently supports Lenovo Ideapad 320s laptops.
2. This version will discard version 0.0.0 BIOS boots and use the wider UEFI boot operating system. On this basis, all functions of version 0.0.0 will run normally on the physical machine.
   New SLAB memory pool and general memory management unit based on SLAB memory pool technology effectively prevent excessive memory fragmentation caused by long time allocation/recycling of available memory.
3. An interrupt processing unit has been added to implement interrupt processing based on APIC. Previous versions of interrupt functionality, based on 8259A PIC, were typical interrupt controller chips in the era of single-core processors. In the era of multi-core processors, APIC can respond faster to interrupt requests and significantly improve the interrupt processing capacity of processors.
4. New keyboard driver to print the input letters in the output window.
5. Multi-core processors under the new SMP system architecture include multi-core boot initialization, multi-core exception handling, multi-core processor locking, and multi-core interrupt handling.

# Version 1.0.1

1. Add timer function, which can generate interrupt regularly.
2. The scheduler uses a CFS scheduling algorithm to schedule tasks within the system. Detect whether a task need scheduled when a timer interrupts.
3. Add  interrupt descriptor tables  for AP processors so that BSP processors and AP processors run tasks simultaneously.