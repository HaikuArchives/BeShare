CC  = gcc
CXX = gcc
CFLAGS = -I../muscle -Isanta -Isanta/BetterScrollView -Isanta/ColumnListView 
CFLAGS += -Isanta/Cursors -Isanta/NewStrings -Isanta/PrefilledBitmap 
CFLAGS += -Isanta/ScrollViewCorner -I../muscle/zlib/zlib 
CFLAGS += -DMUSCLE_ENABLE_ZLIB_ENCODING 

ODIR = obj

CXXFLAGS = $(CFLAGS)
LFLAGS = -L/boot/system/lib/
LIBS = -lbe -lroot -ltranslation -lnetwork -lstdc++
EXECUTABLES = BeShare md5test filechopper
REGEXOBJS = 
SANTAOBJS = BetterScrollView.o CLVColumn.o CLVColumnLabelView.o CLVEasyItem.o 
SANTAOBJS += CLVListItem.o ColumnListView.o Cursors.o NewStrings.o 
SANTAOBJS += PrefilledBitmap.o ScrollViewCorner.o 
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o
ZLIBOBJS += crc32.o compress.o
BESHARE = ReflowingTextView.o ShareNetClient.o ShareFileTransfer.o ChatWindow.o 
BESHARE += ShareApplication.o ShareWindow.o PrivateChatWindow.o 
BESHARE += MessageTransceiverThread.o Thread.o AcceptSocketsThread.o Message.o 
BESHARE += SetupSystem.o String.o main.o SysLog.o AbstractMessageIOGateway.o 
BESHARE += PulseNode.o MessageIOGateway.o PlainTextMessageIOGateway.o 
BESHARE += PathMatcher.o QueryFilter.o ConvertMessages.o RemoteFileItem.o 
BESHARE += RemoteUserItem.o ShareColumn.o ShareMIMEInfo.o 
BESHARE += NetworkUtilityFunctions.o ShareUtils.o SplitPane.o md5.o 
BESHARE += StringMatcher.o ShareStrings.o ReflectServer.o 
BESHARE += AbstractReflectSession.o DumbReflectSession.o 
BESHARE += StorageReflectSession.o DataNode.o ServerComponent.o PathMatcher.o 
BESHARE += StringMatcher.o RateLimitSessionIOPolicy.o ColorPicker.o 
BESHARE += ByteBuffer.o ZLibCodec.o ZLibUtilityFunctions.o 
BESHARE += MiscUtilityFunctions.o SignalHandlerSession.o SignalMultiplexer.o

RESOURCE = BeShare.rsrc

CFLAGS += -Wall -W -Wno-multichar -fno-exceptions -O3

VPATH += ../muscle/message ../muscle/besupport ../muscle/iogateway 
VPATH += ../muscle/reflector ../muscle/regex ../muscle/system ../muscle/util 
VPATH += ../muscle/syslog ../muscle/zlib ../muscle/zlib/zlib santa 
VPATH += santa/BetterScrollView santa/ColumnListView santa/Cursors santa/NewStrings
VPATH += santa/PrefilledBitmap santa/ScrollViewCorner

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

savebeos : CFLAGS += -DSAVE_BEOS
savebeos : RESOURCE = BeShare-sbe.rsrc
savebeos : all

#$(ODIR)/%.o:

BeShare : $(BESHARE) $(SANTAOBJS) $(REGEXOBJS) $(ZLIBOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)
	xres -o $@ $(RESOURCE)
	mimeset -F $@	

md5test : md5.o md5test.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

filechopper : filechopper.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
