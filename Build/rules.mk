# can't define any targets in here, just macros

empty:=
slash=/
underscore=_
slash_to_underscore = $(subst $(slash),$(underscore),$1)
space:=$(empty) $(empty)
first_dot = $(subst $(space),.,$(wordlist 1, 2,$(subst ., ,$(1))))

CPPFLAGS += -Wno-deprecated-declarations
CPPFLAGS += -Wc++11-extensions

ifeq ($(BUILD_TYPE),Release)
CPPFLAGS += -O2
CPPFLAGS += -DNDEBUG=1
else
CPPFLAGS += -fstack-protector-strong
CPPFLAGS += -g -O0
CPPFLAGS += -D_DEBUG=1
endif

CPPFLAGS += -std=c++11 -stdlib=libc++ -Wno-parentheses

LDFLAGS += -Wc++11-extensions

# C++ generated code with dependencies
outName = obj/$(basename $(call slash_to_underscore,$1)).o
depName = obj/$(basename $(call slash_to_underscore,$1)).d

define srcToObj
$(call outName,$1) : obj/dummy $1 $(call depName,$1) Makefile
	$(CXX) $(CPPFLAGS) -MMD $(INCLUDES) -I$(CURDIR) -c -o $(call outName,$1) $1

$(call depName,$1) : $1

# depname
-include $(call depName,$1)

CLEAN += $(call depName,$1)
CLEAN += $(call outName,$1)
endef

# Shader code with dependencies
shaderOutName = obj/Shader/$(notdir $1)
shaderDepName = obj/Shader/$(notdir $1).d

define shaderSrcToObj
$(call shaderOutName,$1) : obj/dummy obj/Shader/dummy $1 $(call shaderDepName,$1) Makefile Build/rules.mk
#	cpp -MMD -IRender/Shaders -MF$(call shaderDepName,$1) $1 $(call shaderOutName,$1)
	-Build/Binary/mcpp -P -@old -MMD -IRender/Shaders -MF $(call shaderDepName,$1) $1 $(call shaderOutName,$1)

$(call shaderDepName,$1) : $1 

# depname
-include $(call shaderDepName,$1)

CLEAN += $(call shaderDepName,$1)
CLEAN += $(call shaderOutName,$1)
endef
