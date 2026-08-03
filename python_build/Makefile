#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules


#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------

# Use gnu-sed if it is macos
ifeq ($(UNAME_S),Darwin)
	SED := gsed
else
	SED := sed
endif

PYCONFIG_DIR := ../python_config

LIBDIRS	:= $(PORTLIBS) $(LIBNX)
export INCLUDE    :=    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
            -I$(CURDIR)/$(BUILD)
CFLAGS    +=    $(INCLUDE) -DSWITCH


ifndef PYVERS
	PYVERS := 3.14.6
endif

ANAME  := libpython$(shell echo $(PYVERS) | $(SED) 's/\([0-9]*\.\([0-9]*\)\).*/\1/').a

OUTDIR := nxpy$(PYVERS)

PYDIR  := Python-$(PYVERS)
PYLINK := https://www.python.org/ftp/python/$(PYVERS)/Python-$(PYVERS).tgz
PYFILE := py.tgz
_NXFILE := _nx.zip
_NXVERS := 5baa28b0ce6ab2c8390fd85cfc36e1086f2cede9
NXFILE := nx.zip
NXVERS := eef890370950b2dec8be797d1e26113cfe457c2f
IMGUIFILE := imgui.zip
IMGUIVERS := 179360f82770850f1af2f03c631d01a9ff839b57



.PHONY: all clean

all: distfPY

distfPY: linkPY
	@[ -d "$(OUTDIR)" ] || mkdir -p $(OUTDIR)
	@[ -d "$(OUTDIR)/include" ] || mkdir -p $(OUTDIR)/include
	@[ -d "$(OUTDIR)/include/nxpy" ] || mkdir -p $(OUTDIR)/include/nxpy
	@[ -d "$(OUTDIR)/lib" ] || mkdir -p $(OUTDIR)/lib
	cp $(ANAME) $(OUTDIR)/lib/
	cp $(PYDIR)/Include/*.h $(OUTDIR)/include/nxpy
	cp $(PYDIR)/Include/cpython/*.h $(OUTDIR)/include/nxpy/cpython
	cp $(PYDIR)/Include/internal/*.h $(OUTDIR)/include/nxpy/internal
	cp $(PYDIR)/pyconfig.h $(OUTDIR)/include/nxpy/
	# Fix socket.py for Switch platform
	cat $(PYDIR)/Lib/socket.py | $(SED) 's/'"'"'getpeername'"'"', //g' >$(PYDIR)/Lib/socket.py.fixed && mv $(PYDIR)/Lib/socket.py.fixed $(PYDIR)/Lib/socket.py
	# Create python.zip with stdlib subset
	cd $(PYDIR)/Lib && \
		ls *.py -1 | xargs zip ../../$(OUTDIR)/python.zip && \
		find json/ encodings/ html/ http/ test/ urllib/ collections/ email/ sqlite3/ logging/ xml/ importlib/ asyncio/ re/ concurrent/ zipfile/ tarfile/ gzip/ bz2/ lzma/ csv/ configparser/ pathlib/ fnmatch/ glob/ shutil/ tempfile/ subprocess/ threading/ multiprocessing/ queue/ socketserver/ select/ selectors/ signal/ struct/ textwrap/ difflib/ pprint/ reprlib/ operator/ functools/ itertools/ contextlib/ abc/ io/ codecs/ unicodedata/ hashlib/ hmac/ secrets/ random/ statistics/ decimal/ fractions/ math/ cmath/ numbers/ datetime/ time/ calendar/ locale/ gettext/ argparse/ optparse/ getpass/ cProfile/ profile/ timeit/ trace/ pdb/ unittest/ doctest/ typing/ dataclasses/ enum/ weakref/ copy/ pickle/ shelve/ dbm/ graphlib/ array/ bisect/ heapq/ keyword/ reprlib/ string/ textwrap/ difflib/ -type f | xargs zip ../../$(OUTDIR)/python.zip

	touch distfPY

linkPY: soospatchPY
	cd $(PYDIR) && make $(MAKEFLAGS) LIBRARY="$(ANAME)" LDLIBRARY="$(ANAME)" $(ANAME) && cd .. && cp $(PYDIR)/$(ANAME) . && touch linkPY

cloneNX:
	wget -O $(_NXFILE) "https://github.com/nx-python/_nx/archive/$(_NXVERS).zip"
	wget -O $(NXFILE) "https://github.com/nx-python/nx/archive/$(NXVERS).zip"
	wget -O $(IMGUIFILE) "https://github.com/nx-python/imgui-switch/archive/$(IMGUIVERS).zip"
	unzip -o $(_NXFILE)
	unzip -o $(NXFILE)
	unzip -o $(IMGUIFILE)
	touch cloneNX

soospatchPY: compilePY
	# Copy platform-specific config files for Python 3.14
	cp $(PYCONFIG_DIR)/pyconfig.h $(PYDIR)/
	cp $(PYCONFIG_DIR)/intrcheck.c $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/pytime.c $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/random.c $(PYDIR)/Modules/
	cp $(PYCONFIG_DIR)/fileutils.c $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/thread_nx.h $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/condvar.h $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/pylifecycle.c $(PYDIR)/Python/
	cp $(PYCONFIG_DIR)/ceval_gil.c $(PYDIR)/Python/
	
	# Patch posixmodule.c for Switch platform
	cp $(PYDIR)/Modules/posixmodule.c $(PYDIR)/Modules/posixmodule.c_old
	cat $(PYDIR)/Modules/posixmodule.c_old | \
		$(SED) 's/return utime(path, time)/errno=ENOENT; return -1/g' | \
		$(SED) 's/define LSTAT lstat/define LSTAT stat/' | \
		$(SED) '1s/^/#include <sys\/socket.h>/' | \
		$(SED) 's/access(path->narrow, mode)/1/' | \
		$(SED) 's/\(^[^rt]*time_t atime, mtime;.*\)/return NULL; \1/' | \
		$(SED) 's/\(^[^ri]*int i = (int)umask(mask);.*\)/int i=0; return NULL;/' | \
		$(SED) 's/^\([^#].*getpwnam_r.*/return NULL;/g' > $(PYDIR)/Modules/posixmodule.c
	
	# Patch socketmodule.c for Switch platform  
	cp $(PYDIR)/Modules/socketmodule.c $(PYDIR)/Modules/socketmodule.c_old
	cat $(PYDIR)/Modules/socketmodule.c_old | \
		$(SED) 's/send(s->sock_fd, buf, len, flags);/send(s->sock_fd, buf, len<4096?len:4096, flags);/g' | \
		$(SED) 's/sizeof(addr->sa_data)/28/g' > $(PYDIR)/Modules/socketmodule.c
	
	# Patch exceptions mapping for Switch platform
	cp $(PYDIR)/Objects/exceptions.c $(PYDIR)/Objects/exceptions.c_old
	cat $(PYDIR)/Objects/exceptions.c_old | \
		$(SED) 's/ESHUTDOWN/EPIPE/g' > $(PYDIR)/Objects/exceptions.c
	
	# Patch Makefile to remove problematic flags and dynamic loading
	cp $(PYDIR)/Makefile $(PYDIR)/Makefile_old
	cat $(PYDIR)/Makefile_old | \
		$(SED) 's/-Werror=declaration-after-statement//' | \
		$(SED) 's/Python\/$$(DYNLOADFILE) \\//\\/' > $(PYDIR)/Makefile
	
	touch soospatchPY

compilePY: patchPY
	cd $(PYDIR) && ./configure CC="$(CC)" CXX="$(CXX)" AS="$(AS)" AR="$(AR)" OBJCOPY="$(OBJCOPY)" STRIP="$(STRIP)" NM="$(NM)" RANLIB="$(RANLIB)" CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" ASFLAGS="$(ASFLAGS)" LDFLAGS="$(LDFLAGS)" CONFIG_SITE="config.site" --disable-shared --with-threads --without-signal-module --disable-ipv6 --host=aarch64-none-elf --build=`./config.guess` --with-build-python && cd .. && touch compilePY

patchPY: cloneNX extractedPY
	# Patch configure to recognize aarch64-none-elf as Linux-like
	cp $(PYDIR)/configure $(PYDIR)/configure_old
	cat $(PYDIR)/configure_old | \
		$(SED) 's/\*\-\*linux\*)/\*\-\*linux\*\|aarch64\-none\-elf)/g' > $(PYDIR)/configure
	
	# Create config.site to override auto-detection for missing features
	echo ac_cv_file__dev_ptmx=no > $(PYDIR)/config.site
	echo ac_cv_file__dev_ptc=no >> $(PYDIR)/config.site
	echo ac_cv_lib_dl_dlopen=no >> $(PYDIR)/config.site
	echo ac_cv_func_accept4=no >> $(PYDIR)/config.site
	echo ac_cv_func_alarm=no >> $(PYDIR)/config.site
	echo ac_cv_func_clock_getres=no >> $(PYDIR)/config.site
	echo ac_cv_func_clock_gettime=no >> $(PYDIR)/config.site
	echo ac_cv_func_confstr=no >> $(PYDIR)/config.site
	echo ac_cv_func_ctermid=no >> $(PYDIR)/config.site
	echo ac_cv_func_dup3=no >> $(PYDIR)/config.site
	echo ac_cv_func_execv=no >> $(PYDIR)/config.site
	echo ac_cv_func_faccessat=no >> $(PYDIR)/config.site
	echo ac_cv_func_fchdir=no >> $(PYDIR)/config.site
	echo ac_cv_func_fchmodat=no >> $(PYDIR)/config.site
	echo ac_cv_func_fchown=no >> $(PYDIR)/config.site
	echo ac_cv_func_fdopendir=no >> $(PYDIR)/config.site
	echo ac_cv_func_getc_unlocked=no >> $(PYDIR)/config.site
	echo ac_cv_func_getentropy=no >> $(PYDIR)/config.site
	echo ac_cv_func_getgrouplist=no >> $(PYDIR)/config.site
	echo ac_cv_func_getgroups=no >> $(PYDIR)/config.site
	echo ac_cv_func_getitimer=no >> $(PYDIR)/config.site
	echo ac_cv_func_getloadavg=no >> $(PYDIR)/config.site
	echo ac_cv_func_getlogin=no >> $(PYDIR)/config.site
	echo ac_cv_func_getnameinfo=no >> $(PYDIR)/config.site
	echo ac_cv_func_getpgid=no >> $(PYDIR)/config.site
	echo ac_cv_func_getpgrp=no >> $(PYDIR)/config.site
	echo ac_cv_func_getpriority=no >> $(PYDIR)/config.site
	echo ac_cv_func_getpwent=no >> $(PYDIR)/config.site
	echo ac_cv_func_getresgid=no >> $(PYDIR)/config.site
	echo ac_cv_func_getresuid=no >> $(PYDIR)/config.site
	echo ac_cv_func_getsid=no >> $(PYDIR)/config.site
	echo ac_cv_func_if_nameindex=no >> $(PYDIR)/config.site
	echo ac_cv_func_inet_aton=no >> $(PYDIR)/config.site
	echo ac_cv_func_initgroups=no >> $(PYDIR)/config.site
	echo ac_cv_func_kill=no >> $(PYDIR)/config.site
	echo ac_cv_func_killpg=no >> $(PYDIR)/config.site
	echo ac_cv_func_lchflags=no >> $(PYDIR)/config.site
	echo ac_cv_func_link=no >> $(PYDIR)/config.site
	echo ac_cv_func_lockf=no >> $(PYDIR)/config.site
	echo ac_cv_func_lseek64=no >> $(PYDIR)/config.site
	echo ac_cv_func_madvise=no >> $(PYDIR)/config.site
	echo ac_cv_func_memfd_create=no >> $(PYDIR)/config.site
	echo ac_cv_func_mkfifo=no >> $(PYDIR)/config.site
	echo ac_cv_func_mkstemp=no >> $(PYDIR)/config.site
	echo ac_cv_func_mlockall=no >> $(PYDIR)/config.site
	echo ac_cv_func_nice=no >> $(PYDIR)/config.site
	echo ac_cv_func_openpty=no >> $(PYDIR)/config.site
	echo ac_cv_func_pselect=no >> $(PYDIR)/config.site
	echo ac_cv_func_readlink=no >> $(PYDIR)/config.site
	echo ac_cv_func_renameat=no >> $(PYDIR)/config.site
	echo ac_cv_func_setegid=no >> $(PYDIR)/config.site
	echo ac_cv_func_seteuid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setgid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setgroups=no >> $(PYDIR)/config.site
	echo ac_cv_func_setitimer=no >> $(PYDIR)/config.site
	echo ac_cv_func_setpgid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setpriority=no >> $(PYDIR)/config.site
	echo ac_cv_func_setregid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setresgid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setresuid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setreuid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setsid=no >> $(PYDIR)/config.site
	echo ac_cv_func_setuid=no >> $(PYDIR)/config.site
	echo ac_cv_func_sigaction=no >> $(PYDIR)/config.site
	echo ac_cv_func_sigaltstack=no >> $(PYDIR)/config.site
	echo ac_cv_func_sigsuspend=no >> $(PYDIR)/config.site
	echo ac_cv_func_socketpair=no >> $(PYDIR)/config.site
	echo ac_cv_func_symlink=no >> $(PYDIR)/config.site
	echo ac_cv_func_sysconf=no >> $(PYDIR)/config.site
	echo ac_cv_func_tcgetpgrp=no >> $(PYDIR)/config.site
	echo ac_cv_func_tgkill=no >> $(PYDIR)/config.site
	echo ac_cv_func_truncate=no >> $(PYDIR)/config.site
	echo ac_cv_func_uname=no >> $(PYDIR)/config.site
	echo ac_cv_func_unlinkat=no >> $(PYDIR)/config.site
	echo ac_cv_func_utimes=no >> $(PYDIR)/config.site
	echo ac_cv_func_waitpid=no >> $(PYDIR)/config.site
	
	# Configure Modules/Setup for Switch platform
	cp $(PYDIR)/Modules/Setup.stdlib.in $(PYDIR)/Modules/Setup.dist
	cat $(PYDIR)/Modules/Setup.dist | \
		$(SED) 's/^#\(array\|cmath\|math\|_struct\|operator\|_random\|_collections\|itertools\|strop\|unicodedata\|_io\|_csv\|_md5\|_sha1\|_sha256\|_sha3\|binascii\|select\|time\|_functools\|_socket\|datetime\|_bisect\|zlib\|_blake2\|_pickle\|_statistics\|_heapq\|_operator\)/\1/' | \
		$(SED) 's/^#\(pwdmodule.c\|grpmodule.c\|posixmodule.c\).*/#\1/' > $(PYDIR)/Modules/Setup.dist.tmp && mv $(PYDIR)/Modules/Setup.dist.tmp $(PYDIR)/Modules/Setup.dist
	
	# Add nx module to Setup
	echo "nx -I$$(srcdir)/Modules/_nx _nx/_nxmodule.c hashtable.c" >> $(PYDIR)/Modules/Setup.dist
	cat imgui-switch-*/setup.txt >> $(PYDIR)/Modules/Setup.dist
	
	cp -r _nx-*/_nx $(PYDIR)/Modules/
	cp -r imgui-switch-*/dist/modules/* $(PYDIR)/Modules/
	
	touch patchPY

extractedPY: $(PYFILE)
	tar xfzv $(PYFILE) && touch extractedPY

$(PYFILE): 
	wget -O "$(PYFILE)" "$(PYLINK)" || curl -Lo "$(PYFILE)" "$(PYLINK)"

clean:
	@rm -rf $(PYDIR) $(PYFILE) $(_NXFILE) $(NXFILE) patchPY extractedPY compilePY linkPY distfPY soospatchPY cloneNX _nx-* nx-* imgui-switch-* libpython*.a
