/*
 * operations on IDE disk.
 * 与磁盘镜像进行交互
 */

#include "fs.h"
#include "lib.h"
#include <mmu.h>

// syscalls to be used
// int syscall_write_dev(u_int va, u_int dev, u_int len);
// int syscall_read_dev(u_int va, u_int dev, u_int len);

// Overview:
// 	read data from IDE disk. First issue a read request through
// 	disk register and then copy data from disk buffer
// 	(512 bytes, a sector) to destination array.
//
// Parameters:
//	diskno: disk number.
// 	secno: start sector number.
// 	dst: destination for data read from IDE disk.
// 	nsecs: the number of sectors to read.
//
// Post-Condition:
// 	If error occurrs during the read of the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
/*** exercise 5.2 ***/
void
ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs)
{
	// 0x200: the size of a sector: 512 bytes.
	int offset_begin = secno * 0x200;
	int offset_end = offset_begin + nsecs * 0x200;
	int offset = 0;

	u_int dev_phy_addr = 0x13000000;
	u_int read_flag = 0;

	while (offset_begin + offset < offset_end) {
		// Your code here
		// error occurred, then panic.
		u_int current_offset = offset_begin + offset;
		if (syscall_write_dev((u_int)&diskno, dev_phy_addr + 0x10, 4) != 0){                                  // 将diskno写入到0xB3000010处，这样就表示我们将使用编号为diskno的磁盘
			panic("ide_read error");
		}
		if(syscall_write_dev((u_int)&current_offset, dev_phy_addr, 4) != 0){                                 // 将相对于磁盘起始位置的offset写入到0xB3000000位置，表示在距离磁盘起始处offset的位置开始进行磁盘操作。
			panic("ide_read error");
		}
		if(syscall_write_dev((u_int)&read_flag, dev_phy_addr + 0x20, 4) != 0){                               // 向内存0xB3000020处写入0来开始读磁盘
			panic("ide_read error");
		}
		u_int success = 0;
		if(syscall_read_dev((u_int)&success, dev_phy_addr + 0x30, 4) != 0){                                   // 从0xB3000030处获取写磁盘操作的返回值，通过判断read_sector函数的返回值，就可以知道读取磁盘的操作是否成功。
			panic("ide_read error");
		}
		if(!success){
			panic("ide_read error");
		}
		if(syscall_read_dev((u_int)(dst + current_offset), dev_phy_addr + 0x4000, 0x200) != 0){                // 如果成功，将这个sector的数据(512 bytes)从设备缓冲区(offset 0x4000-0x41ff)中拷贝到目的位置。
			panic("ide_read error");
		}
		offset += 0x200;
	}
}


// Overview:
// 	write data to IDE disk.
//
// Parameters:
//	diskno: disk number.
//	secno: start sector number.
// 	src: the source data to write into IDE disk.
//	nsecs: the number of sectors to write.
//
// Post-Condition:
//	If error occurrs during the read of the IDE disk, panic.
//
// Hint: use syscalls to access device registers and buffers
/*** exercise 5.2 ***/
void
ide_write(u_int diskno, u_int secno, void *src, u_int nsecs)
{
	// Your code here
	int offset_begin = secno * 0x200;
	int offset_end = offset_begin + nsecs * 0x200;
	int offset = 0;

	u_int dev_phy_addr = 0x13000000;
	u_int write_flag = 1;

	// DO NOT DELETE WRITEF !!!
	writef("diskno: %d\n", diskno);

	while (offset_begin + offset < offset_end) {
		// Your code here
		// error occurred, then panic.
		u_int current_offset = offset_begin + offset;
		if (syscall_write_dev((u_int)&diskno, dev_phy_addr + 0x10, 4) != 0){                                // 将diskno写入到0xB3000010处，这样就表示我们将使用编号为diskno的磁盘
			panic("ide_read error");
		}
		if(syscall_write_dev((u_int)&current_offset, dev_phy_addr, 4) != 0){                                // 将相对于磁盘起始位置的offset写入到0xB3000000位置，表示在距离磁盘起始处offset的位置开始进行磁盘操作。
			panic("ide_read error");
		}
		if(syscall_read_dev((u_int)(src + current_offset), dev_phy_addr + 0x4000, 0x200) != 0){             // 如果成功，将这个sector的数据(512 bytes)从设备缓冲区(offset 0x4000-0x41ff)中拷贝到目的位置。
			panic("ide_read error");
		}
		if(syscall_write_dev((u_int)&write_flag, dev_phy_addr + 0x20, 4) != 0){                             // 向内存0xB3000020处写入0来开始读磁盘
			panic("ide_read error");
		}
		u_int success = 0;
		if(syscall_read_dev((u_int)&success, dev_phy_addr + 0x30, 4) != 0){                                 // 从0xB3000030处获取写磁盘操作的返回值，通过判断read_sector函数的返回值，就可以知道读取磁盘的操作是否成功。
			panic("ide_read error");
		}
		if(!success){
			panic("ide_read error");
		}
		offset += 0x200;
	}
}
