include build/linux/colors

export SRC_PATH  := src
export OBJ_DIR   := obj_$(TIM_PLATFORM)

ifeq ($(TIM_SILENT), 1)
	export AT := @
else
	export AT :=
endif

export SUBDIRS := $(shell find $(SRC_PATH) -type d -execdir test -f {}/IGNORE ';' -prune -o \( -type d -print \))

export INCLUDES := $(addprefix -I, $(SUBDIRS)) $(TIM_INCLUDES)

ifdef TIM_DEBUG_MODE
	TIM_DEFINES  := -DTIM_OS_LINUX -DTIM_DEBUG
	TIM_CFLAGS   := -g -O0 -Wall -Werror
	TIM_CPPFLAGS := -g -O0 -fno-exceptions -Wall -Werror
	TIM_STRIP    := touch
else
	TIM_DEFINES  := -DTIM_OS_LINUX
	TIM_CFLAGS   := -O3 -Wall -Werror
	TIM_CPPFLAGS := -O3 -fno-exceptions -Wall -Werror
	TIM_STRIP    := $(STRIP)
endif

TIM_LIBS := -lm

FORT_DEFINES := -DFT_CONGIG_DISABLE_WCHAR # Actually, we need this on Windows only.
JSON_DEFINES := -DJSON_NOEXCEPTION=1 -DJSON_DIAGNOSTICS=1 -DJSON_DIAGNOSTIC_POSITIONS=1
LIL_DEFINES := -DLIL_ENABLE_POOLS -DLIL_ENABLE_RECLIMIT=1024
MONGOOSE_DEFINES := -DMG_TLS=MG_TLS_MBED

SQLITE3_DEFINES := \
    -DSQLITE_THREADSAFE=0 \
    -DSQLITE_DEFAULT_MEMSTATUS=0 \
    -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 \
    -DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
    -DSQLITE_MAX_EXPR_DEPTH=0 \
    -DSQLITE_OMIT_DECLTYPE \
    -DSQLITE_OMIT_DEPRECATED \
    -DSQLITE_OMIT_PROGRESS_CALLBACK \
    -DSQLITE_OMIT_SHARED_CACHE \
    -DSQLITE_USE_ALLOCA \
    -DHAVE_FDATASYNC=1 \
    -DHAVE_GMTIME_R=1 \
    -DHAVE_ISNAN=1 \
    -DHAVE_LOCALTIME_R=1 \
    -DHAVE_LOCALTIME_S=1 \
    -DHAVE_MALLOC_USABLE_SIZE=1 \
    -DHAVE_STRCHRNUL=1 \
    -DHAVE_USLEEP=1 \
    -DHAVE_UTIME=1 \
    -DSQLITE_BYTEORDER=1234 \
    -DSQLITE_DEFAULT_CACHE_SIZE=16384 \
    -DSQLITE_DEFAULT_FOREIGN_KEYS=1 \
    -DSQLITE_DEFAULT_PAGE_SIZE=4096 \
    -DSQLITE_TEMP_STORE=3 \
    -DSQLITE_USE_URI=1 \
    -DSQLITE_ENABLE_BATCH_ATOMIC_WRITE \
    -DSQLITE_ENABLE_UPDATE_DELETE_LIMIT=1 \
    -DSQLITE_CORE

DEFINES := $(TIM_DEFINES) $(FORT_DEFINES) $(JSON_DEFINES) $(LIL_DEFINES) $(MONGOOSE_DEFINES) $(SQLITE3_DEFINES)
CFLAGS   := $(TIM_CFLAGS)
CPPFLAGS := $(TIM_CPPFLAGS)

export LIBS := -Xlinker --start-group $(TIM_LIBS) -Xlinker --end-group

C_SRCS := $(shell find $(SRC_PATH) -type f -name *.c -execdir test ! -f IGNORE ';' -print)
CPP_SRCS := $(shell find $(SRC_PATH) -type f -name *.cpp -execdir test ! -f IGNORE ';' -print)

VPATH := $(SUBDIRS)

all:	tim

### Load dependencies
### -----------------
DEPS := $(wildcard $(OBJ_DIR)/*.d)
ifneq ($(strip $(DEPS)),)
include $(DEPS)
endif

### Compilation and dependencies generation
### ---------------------------------------

define COMPILE_C
@echo $(TEXT_BG_BLUE)$(TEXT_FG_BOLD_YELLOW)" C "$(TEXT_NORM)$(TEXT_FG_BOLD_CYAN)$< $(TEXT_NORM)
$(AT)$(CC) -c -MD $(CFLAGS) $(if $(findstring 3rdparty,$(dir $<)),$(CFLAGS_3RD),) $(INCLUDES) $(DEFINES) -o $@ $<
endef

define COMPILE_CPP
@echo $(TEXT_BG_BLUE)$(TEXT_FG_BOLD_YELLOW)"C++"$(TEXT_NORM)$(TEXT_FG_BOLD_CYAN)$< $(TEXT_NORM)
$(AT)$(CPP) -c -MD $(CPPFLAGS) $(if $(findstring 3rdparty,$(dir $<)),$(CFLAGS_3RD),) $(INCLUDES) $(DEFINES) -o $@ $<
endef

define DEPENDENCIES_C
@if [ ! -f $(@D)/$(<F:.c=.d) ]; then \
    sed 's/^$(@F):/$(@D)\/$(@F):/g' < $(<F:.c=.d) > $(@D)/$(<F:.c=.d); \
    rm -f $(<F:.c=.d); \
fi
endef

define DEPENDENCIES_CPP
@if [ ! -f $(@D)/$(<F:.cpp=.d) ]; then \
    sed 's/^$(@F):/$(@D)\/$(@F):/g' < $(<F:.cpp=.d) > $(@D)/$(<F:.cpp=.d); \
    rm -f $(<F:.cpp=.d); \
fi
endef

C_OBJS   := $(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(C_SRCS)))
CPP_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(CPP_SRCS)))

.SECONDARY:	$(C_OBJS) $(CPP_OBJS)
.PHONY:	clean build-time

### Target rules
### ------------

build-time:
		$(AT)printf '"%s"' $(shell date '+%Y%m%d-%H%M') > BUILD_TIME

$(OBJ_DIR)/%.o: %.cpp
		$(COMPILE_CPP)
		$(DEPENDENCIES_CPP)

$(OBJ_DIR)/%.o: %.c
		$(COMPILE_C)
		$(DEPENDENCIES_C)

tim: build-time $(OBJ_DIR) $(C_OBJS) $(CPP_OBJS)
		@echo $(TEXT_BG_GREEN)$(TEXT_FG_BLACK)" L "$(TEXT_NORM)$(TEXT_FG_BOLD_GREEN)$@ $(TEXT_NORM)
		@rm -f $@
		$(AT)$(CPP) $(CFLAGS) $(DEFINES) -o $@ $(C_OBJS) $(CPP_OBJS) $(LIBS)
		$(AT)$(TIM_STRIP) $@

$(OBJ_DIR):
		$(AT)mkdir $(OBJ_DIR)

clean:
		@echo $(TEXT_FG_LIGHT_GREEN)"> Cleaning ... "$(TEXT_NORM)
		$(AT)rm -rf $(OBJ_DIR)
		$(AT)rm -f *~ *.d *.gdb core* *.so *.a *.log BUILD_TIME .DS_Store tim
		@echo $(TEXT_FG_LIGHT_GREEN)"> Done :) "$(TEXT_NORM)
