CC  = gcc
CXX = gcc
CFLAGS = -I../muscle -Isanta -Isanta/BetterScrollView -Isanta/ColumnListView -Isanta/Cursors -Isanta/NewStrings -Isanta/PrefilledBitmap -Isanta/ScrollViewCorner -I../muscle/zlib/zlib -DMUSCLE_ENABLE_ZLIB_ENCODING -D_ZETA_USING_DEPRECATED_API_ -D_ZETA_TS_FIND_DIR_

CXXFLAGS = $(CFLAGS)
LFLAGS = 
LIBS = 
EXECUTABLES = BeShare.$(BE_HOST_CPU) md5test filechopper
REGEXOBJS = 
SANTAOBJS = BetterScrollView.o CLVColumn.o CLVColumnLabelView.o CLVEasyItem.o CLVListItem.o ColumnListView.o Cursors.o NewStrings.o PrefilledBitmap.o ScrollViewCorner.o 
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o
RESOURCE = BeShare.rsrc

ifeq ($(OSTYPE),beos)
  ifeq ($(BE_HOST_CPU),ppc)
    CXX = mwcc 
    CC = mwcc 
    CXXFLAGS += -DMUSCLE_MAX_OUTPUT_CHUNK=1000 -Dstd=""
    REGEXOBJS = regcomp.o regexec.o regerror.o regfree.o
    VPATH += ../muscle/regex/regex
    CFLAGS += -I../muscle/regex/regex
    LIBS = -ltranslation
  else
    CFLAGS += -Wall -W -Wno-multichar -fno-exceptions -O2  # faster than -O3!!
    LIBS = -lbe -lnet -lroot -ltranslation
    ifeq ($(shell ls 2>/dev/null -1 /boot/develop/headers/be/bone/bone_api.h), /boot/develop/headers/be/bone/bone_api.h)
      CFLAGS += -I/boot/develop/headers/be/bone -DBONE
      LIBS = -nodefaultlibs -lbind -lsocket -lbe -lroot -ltranslation -L/boot/beos/system/lib
    else
      CXXFLAGS += -DMUSCLE_MAX_OUTPUT_CHUNK=1000
    endif
  endif
endif

VPATH += ../muscle/message ../muscle/besupport ../muscle/iogateway ../muscle/reflector ../muscle/regex ../muscle/system ../muscle/util ../muscle/syslog ../muscle/zlib ../muscle/zlib/zlib santa santa/BetterScrollView santa/ColumnListView santa/Cursors santa/NewStrings santa/PrefilledBitmap santa/ScrollViewCorner

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

savebeos : CFLAGS += -DSAVE_BEOS
savebeos : RESOURCE = BeShare-sbe.rsrc
savebeos : all

BeShare.$(BE_HOST_CPU) : ReflowingTextView.o ShareNetClient.o ShareFileTransfer.o ChatWindow.o ShareApplication.o ShareWindow.o PrivateChatWindow.o MessageTransceiverThread.o Thread.o AcceptSocketsThread.o Message.o SetupSystem.o String.o main.o SysLog.o AbstractMessageIOGateway.o PulseNode.o MessageIOGateway.o PlainTextMessageIOGateway.o PathMatcher.o QueryFilter.o ConvertMessages.o RemoteFileItem.o RemoteUserItem.o ShareColumn.o ShareMIMEInfo.o NetworkUtilityFunctions.o ShareUtils.o SplitPane.o md5.o StringMatcher.o ShareStrings.o ReflectServer.o AbstractReflectSession.o DumbReflectSession.o StorageReflectSession.o DataNode.o ServerComponent.o PathMatcher.o StringMatcher.o RateLimitSessionIOPolicy.o ColorPicker.o ByteBuffer.o ZLibCodec.o ZLibUtilityFunctions.o $(SANTAOBJS) $(REGEXOBJS) $(ZLIBOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)
	xres -o $@ $(RESOURCE)
	mimeset -F $@	

md5test : md5.o md5test.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

filechopper : filechopper.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
