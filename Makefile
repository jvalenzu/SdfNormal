# Makefile-mode
include Build/rules.mk

MAX_KERNEL_WIDTH := 23

CPPFLAGS += -IExternal
CPPFLAGS += -I.
CPPFLAGS += -DMAX_KERNEL_WIDTH=$(MAX_KERNEL_WIDTH)

CLC = /System/Library/Frameworks/OpenCL.framework/Libraries/openclc

SRCS += main.c
SRCS += cda.c
SRCS += dr.c
SRCS += blur.c
SRCS += cl.c
SRCS += External/lodepng.c
OBJS := $(foreach src,$(SRCS),$(call outName,$(src)))

all: SdfNormal

SdfNormal: pvrt.cl.x86_64.bc pvrt.cl.gpu_64.bc $(OBJS) Makefile
	$(CXX) -o $@ $(CPPFLAGS) $(SRCS) -framework OpenCL

%.x86_64.bc: pvrt.cl gen_src/pvrt.cl.inc
	$(CLC) -x cl -cl-std=CL1.1 -Os -arch x86_64 -emit-llvm -c -o $@ $(call first_dot,$@)

%.gpu_64.bc: pvrt.cl gen_src/pvrt.cl.inc
	$(CLC) -x cl -cl-std=CL1.1 -Os -arch gpu_64 -emit-llvm -c -o $@ $(call first_dot,$@)

gen_src/pvrt.cl.inc : gen_src blurGen.pl
	perl blurGen.pl $(MAX_KERNEL_WIDTH) > $@

gen_src :
	mkdir gen_src

info:
	$(CLC) -v

$(foreach src,$(SRCS),$(eval $(call srcToObj,$(src))))

CLEAN += SdfNormal

include Build/rules2.mk
