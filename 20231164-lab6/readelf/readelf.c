/* This is a simplefied ELF reader.
 * You can contact me if you find any bugs.
 *
 * Luming Wang<wlm199558@126.com>
 */

#include "kerelf.h"
#include <stdio.h>
/* Overview:
 *   Check whether it is a ELF file.
 *
 * Pre-Condition:
 *   binary must longer than 4 byte.
 *
 * Post-Condition:
 *   Return 0 if `binary` isn't an elf. Otherwise
 * return 1.
 */
#define REVERSE_32(n_n) \
        ((((n_n)&0xff) << 24) | (((n_n)&0xff00) << 8) | (((n_n) >> 8) & 0xff00) | (((n_n) >> 24) & 0xff))
#define REVERSE_16(n_n) \
	((((n_n)&0xff) << 8) | (((n_n) >> 8) & 0xff))

int is_elf_format(u_char *binary)
{
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)binary;
        if (ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
                ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
                ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
                ehdr->e_ident[EI_MAG3] == ELFMAG3) {
                return 1;
        }

        return 0;
}

/* Overview:
 *   read an elf format binary file. get ELF's information
 *
 * Pre-Condition:
 *   `binary` can't be NULL and `size` is the size of binary.
 *
 * Post-Condition:
 *   Return 0 if success. Otherwise return < 0.
 *   If success, output address of every section in ELF.
 */

/*
    Exercise 1.2. Please complete func "readelf". 
*/
int readelf(u_char *binary, int size)
{
        Elf32_Ehdr *ehdr = (Elf32_Ehdr *)binary; 
        
        // check whether `binary` is a ELF file.
        if (size < 4 || !is_elf_format(binary)) {
                printf("not a standard elf format\n");
                return -1;
        }  

        unsigned char e_iden = (ehdr -> e_ident)[5];

    
        if(e_iden == 1){  // LSB
//            printf("\nLSB Shdr\n");
            // Shdr
            int numOfSections;
            Elf32_Shdr *shdr = NULL;
            u_char *ptr_sh_table = NULL;
            Elf32_Half sh_entry_count;
            Elf32_Half sh_entry_size;

            // get section table addr, section header number and section header size.
            ptr_sh_table = binary + ehdr -> e_shoff;  // binary指向的地址+ehdr->e_shoff shdr相对elf头的偏移量就是shdr的首地址
            sh_entry_count = ehdr -> e_shnum;  // shdr的数量
            sh_entry_size = ehdr -> e_shentsize;  // shdr每块的大小
            // for each section header, output section number and section addr. 
            // hint: section number starts at 0.
            for(numOfSections = 0;numOfSections < sh_entry_count; numOfSections++){
                shdr = (Elf32_Shdr *)(ptr_sh_table + numOfSections * sh_entry_size);  // 将ptr_sh_table当前指向的地址转化为shdr类型
                printf("%d:0x%x\n", numOfSections, shdr -> sh_addr);  // 输出个数及shdr地址
            }   
			return 0;
/*
            printf("\nLSB Phdr\n");

            // Phdr
            int numOfPrograms;
            Elf32_Phdr *phdr = NULL;
            u_char *ptr_ph_table = NULL;
            Elf32_Half ph_entry_count;
            Elf32_Half ph_entry_size;

            ptr_ph_table = binary + ehdr -> e_phoff;
            ph_entry_count = ehdr -> e_phnum;
            ph_entry_size = ehdr -> e_phentsize;

            for(numOfPrograms = 0;numOfPrograms < ph_entry_count;numOfPrograms++){
                phdr = (Elf32_Phdr *)(ptr_ph_table + numOfPrograms * ph_entry_size);
                printf("%d: filesize:0x%x, memorysize:0x%x\n", numOfPrograms, phdr -> p_filesz, phdr -> p_memsz);
            }

        }else if(e_iden == 2){
            printf("\nMSB Shdr\n");
            // Shdr
            int numOfSections;
            Elf32_Shdr *shdr = NULL;
            u_char *ptr_sh_table = NULL;
            Elf32_Half sh_entry_count;
            Elf32_Half sh_entry_size;

            // get section table addr, section header number and section header size.
            ptr_sh_table = binary + REVERSE_32(ehdr -> e_shoff);  // binary指向的地址+ehdr->e_shoff shdr相对elf头的偏移量就是shdr的首地址
			sh_entry_count = REVERSE_16(ehdr -> e_shnum);  // shdr的数量
			sh_entry_size = REVERSE_16(ehdr -> e_shentsize);  // shdr每块的大小
		   	// for each section header, output section number and section addr. 
            // hint: section number starts at 0.
            printf("sh_entry_count: %x\n",sh_entry_count);
			printf("sh_entry_size: %x\n",sh_entry_size);
			for(numOfSections = 0;numOfSections < sh_entry_count; numOfSections++){
                shdr = (Elf32_Shdr *)(ptr_sh_table + numOfSections * sh_entry_size);  // 将ptr_sh_table当前指向的地址转化为shdr类型
                printf("%d:0x%x\n", numOfSections, REVERSE_32(shdr -> sh_addr));  // 输出个数及shdr地址
            }   

            printf("\nMSB Phdr\n");

            // Phdr
            int numOfPrograms;
            Elf32_Phdr *phdr = NULL;
            u_char *ptr_ph_table = NULL;
            Elf32_Half ph_entry_count;
            Elf32_Half ph_entry_size;

            ptr_ph_table = binary + REVERSE_32(ehdr -> e_phoff);
            ph_entry_count = REVERSE_16(ehdr -> e_phnum);
            ph_entry_size = REVERSE_16(ehdr -> e_phentsize);

            for(numOfPrograms = 0;numOfPrograms < ph_entry_count;numOfPrograms++){
                phdr = (Elf32_Phdr *)(ptr_ph_table + numOfPrograms * ph_entry_size);
                printf("%d: filesize:0x%x, memorysize:0x%x\n", numOfPrograms, REVERSE_32(phdr -> p_filesz), REVERSE_32(phdr -> p_memsz));
            }
  */      }

        return 0;
}
