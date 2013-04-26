PROG     = xmlsample
OBJS     = xmlsample.o

INCLUDE =-I./
CFLAGS  = -O2 $(shell pkg-config --cflags gstreamer-controller-0.10) $(INCLUDE)
LDLIBS  = $(shell pkg-config --libs gstreamer-controller-0.10)

.PHONY: clean

$(PROG): $(OBJS)
	$(CC) $^ $(LDLIBS) -o $@

%.o:%.c
	$(CC) $(CFLAGS) -c $<

deps:
	$(CC) -MM $(CCFLAGS) $(SRCS) > $(DEPFILE)

clean:
	rm $(OBJS) $(PROG)