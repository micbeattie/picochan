INCLUDE_FLAGS=-I $(PICOCHAN_PATH)/base/include \
	-I $(PICOCHAN_PATH)/css/include \
	-I $(PICOCHAN_PATH)/base/proto
CDEBUGFLAGS=-g
CFLAGS=-Wall $(INCLUDE_FLAGS) $(CDEBUGFLAGS)

OBJS=pch_dump_trace.o format.o

pch_dump_trace: $(OBJS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) pch_dump_trace
