# targets can go in here.

%.x86_64.bc: pvrt.cl gen_src/pvrt.cl.inc
	$(CLC) -x cl -cl-std=CL1.1 -Os -arch x86_64 -emit-llvm -c -o $@ $(call first_dot,$@)

.PHONY: clean

clean:
	-rm -f $(CLEAN)


obj/dummy:
	-mkdir obj
	touch obj/dummy

obj/Shader/dummy: obj/dummy
	-mkdir obj/Shader
	touch obj/Shader/dummy
