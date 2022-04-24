# Main makefile
#
# Copyright (C) 2007 Beihang University
# Written by Zhu Like ( zlike@cse.buaa.edu.cn )
#

drivers_dir	  := drivers
boot_dir	  := boot
init_dir	  := init
lib_dir		  := lib
tools_dir	  := tools
test_dir	  := 
#上面定义了各种文件夹名称
vmlinux_elf	  := gxemul/vmlinux
#这个是我们最终需要生成的elf文件
link_script   := $(tools_dir)/scse0_3.lds #链接用的脚本

modules		  := boot drivers init lib
objects		  := $(boot_dir)/start.o			  \
				 $(init_dir)/main.o			  \
				 $(init_dir)/init.o			  \
			   	 $(drivers_dir)/gxconsole/console.o \
				 $(lib_dir)/*.o				  \
#定义了需要生成的各种文件

.PHONY: all $(modules) clean

all: $(modules) vmlinux #我们的“最终目标”

vmlinux: $(modules)
	$(LD) -o $(vmlinux_elf) -N -T $(link_script) $(objects)

##注意,这里调用了一个叫做$(LD)的程序

$(modules): 
	$(MAKE) --directory=$@

#进入各个子文件夹进行make

clean: 
	for d in $(modules);	\
		do					\
			$(MAKE) --directory=$$d clean; \
		done; \
	rm -rf *.o *~ $(vmlinux_elf)

include include.mk
