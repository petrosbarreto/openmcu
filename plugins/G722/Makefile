
AC_PLUGIN_DIR=codecs/audio
prefix=/opt/openmcu-10rm/lib/ptlib
target_os=linux-gnu

SONAME	= g722

SRCDIR	= ./VoIPCodecs
OBJDIR	= ./obj
PLUGINDIR=../..

CC		=gcc
CXX		=g++
LDSO		=-shared -Wl,-soname,$(SONAME)
PLUGINEXT	=so
CFLAGS	       +=-g -O2 
STDCCFLAGS     +=-fPIC -D__STDC_CONSTANT_MACROS
LDFLAGS	       +=
EXTRACFLAGS     =-I$(PLUGINDIR)

SRCS	+= g722codec.cpp \
	   g722_encode.c \
	   g722_decode.c \
	   bitstream.c 

vpath	%.o $(OBJDIR)
vpath	%.c $(SRCDIR)
vpath	%.cpp .

ifeq ($(VERBOSE),)
Q_CC = @echo [CC] `echo $< | sed s^@OPALDIR@/^^` ;
Q_LD = @echo [LD] `echo $(PLUGIN) | sed s^@OPALDIR@/^^` ;
endif


$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(Q_CC)$(CC) -I../../../include $(STDCCFLAGS) $(OPTCCFLAGS) $(EXTRACFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o : %.cpp
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(CXX) $(EXTRACCFLAGS) $(STDCCFLAGS) $(OPTCCFLAGS) $(CFLAGS) -c $< -o $@

PLUGIN	= ./g722_audio_pwplugin.$(PLUGINEXT)

OBJECTS = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(notdir $(SRCS)))))

all: $(PLUGIN)

$(PLUGIN): $(OBJECTS)
ifeq (solaris,$(findstring solaris,$(target_os)))

	$(Q_LD)$(CXX) $(LDSO) $@ -o $@ $^ $(EXTRALIBS)
else
	$(Q_LD)$(CXX) $(LDSO) -o $@ $^ $(EXTRALIBS)

endif	

install:
	mkdir -p $(DESTDIR)$(prefix)/$(AC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(prefix)/$(AC_PLUGIN_DIR)

uninstall:
	rm -f $(DESTDIR)$(prefix)/$(AC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

###########################################

