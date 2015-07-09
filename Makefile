CC	= gcc
#CFLAGS	=
#CFLAGS	= -DLATEX
CFLAGS	= -DTOKEN_HTML
LFLAGS	=

OBJS	= codegen.o \
	  compile.o \
	  getSource.o \
	  main.o \
	  table.o

.SUFFIXES	: .o .c

.c.o	:
	$(CC) $(CFLAGS) -c $<

pl0d	: ${OBJS}
	$(CC) -o $@ ${OBJS}

clean	:
	\rm -rf *~ *.o

tags:
	etags *.c *.h
