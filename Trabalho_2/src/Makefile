CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic
DEPS= constants.h aux.h types.h sope.h user.h server.h
OBJS= log.o aux.o 

.PHONY: all clean
all: server user

server: $(OBJS) server.o
	$(CC) $(CFLAGS) $^ -o $@

user: $(OBJS) user.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -MMD -c $< -o $@
	
clean:
	rm -f $(TARGETS) *.o *.d *.i *.s
