ifeq ($(APPLICATION),lib)
	ifneq ($(STYLE),RELEASE)
        $(error library can only be built in RELEASE style)
	endif
endif

MODULES := \
	apps/$(APPLICATION) \
	sys

ifeq ($(APPLICATION),app)
	MODULES += \
		drivers/serial \
		drivers/random
endif

GET_LOCAL_DIR = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

OPTIONS :=
OBJECTS :=

CFLAGS := 

include $(GET_LOCAL_DIR)/platforms/$(PLATFORM).mk

PYTHON ?= python3

CFLAGS += $(ARCHS)
CFLAGS += $(OS_MIN_VERSION)
CFLAGS += -O3
CFLAGS += -Iinclude
CFLAGS += -MMD
CFLAGS += -DPRODUCT_NAME=\"$(PROJ_NAME)\"

CODESIGN := codesign
CODESIGN_FLAGS := -s - -f

ifeq ($(APPLICATION),lib)
	CFLAGS += -DBUILDING_POLINA_LIBRARY=1
endif

ifeq ($(APPLICATION),app)
	LD := $(CC)
	LDFLAGS += $(ARCHS)
	LDFLAGS += $(OS_MIN_VERSION)
	LDFLAGS += -framework CoreFoundation
	LDFLAGS += -framework IOKit
	LDFLAGS += -sectcreate __TEXT __polina_tag $(BUILD_TAG_FILE)
	LDFLAGS += -Wl,-no_adhoc_codesign
else ifeq ($(APPLICATION),lib)
	LD := xcrun libtool
endif

ifeq ($(STYLE),ASAN)
	CFLAGS += -fsanitize=address
	LDFLAGS += -fsanitize=address
	CFLAGS += -gfull
	LDFLAGS += -gfull
else ifeq ($(STYLE),PROFILING)
	CFLAGS += -gfull
	LDFLAGS += -gfull
	CODESIGN_FLAGS += --entitlements gta.entitlements
else ifneq ($(STYLE),RELEASE)
    $(error unknown STYLE)
endif

CURRENT_ROOT := $(BUILD_ROOT)/$(APPLICATION)/$(PLATFORM)/$(STYLE)

DIR_HELPER = mkdir -p $(@D)

ifeq ($(APPLICATION),app)
	BASE 		:= $(BUILD_ROOT)/$(APPLICATION)/$(PLATFORM)/$(PROJ_NAME)
	EXTENSION 	:=
else ifeq ($(APPLICATION),lib)
	BASE 		:= $(BUILD_ROOT)/$(APPLICATION)/$(PLATFORM)/lib$(PROJ_NAME)
	EXTENSION 	:= ".a"
endif

ifeq ($(STYLE),RELEASE)
	SUFFIX :=
else
	SUFFIX := -$(STYLE)
endif

BINARY := $(BASE)$(SUFFIX)$(EXTENSION)

MODULES_INCLUDES := $(addsuffix /rules.mk,$(MODULES))

-include $(MODULES_INCLUDES)

.PHONY: clean all

all: $(BINARY)
	@echo "%%%%% done building"

$(BINARY): $(OBJECTS)
	@echo "\tlinking"
	@$(DIR_HELPER)
	@$(LD) $(LDFLAGS) $^ -o $@
	@echo "\tcodesigning"
	@$(CODESIGN) $(CODESIGN_FLAGS) $@

$(CURRENT_ROOT)/%.o: %.c
	@echo "\tcompiling C: $<"
	@$(DIR_HELPER)
	@$(CC) $(CFLAGS) $(OPTIONS) -c $< -o $@ 

-include $(OBJECTS:.o=.d)
