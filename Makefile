EXEC = \
	test-async \
	test-reactor \
	test-buffer \
	test-protocol-server \
	httpd

OUT ?= .build
.PHONY: all

CC ?= gcc
CFLAGS = -std=gnu99 -Wall -O2 -g -I .
LDFLAGS = -lpthread

HTTP_PARSER_PATH := externals/http-parser
CFLAGS += -I./$(HTTP_PARSER_PATH)
LDFLAGS += -L./$(HTTP_PARSER_PATH) -lhttp_parser

OBJS := \
	async.o \
	reactor.o \
	buffer.o \
	protocol-server.o
deps := $(OBJS:%.o=%.o.d)
OBJS := $(addprefix $(OUT)/,$(OBJS))
deps := $(addprefix $(OUT)/,$(deps))

all: $(HTTP_PARSER_PATH)/libhttp_parser.a $(OUT) $(EXEC)

$(HTTP_PARSER_PATH)/libhttp_parser.a:
	make -C $(HTTP_PARSER_PATH) package

httpd: $(OBJS) httpd.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-%: $(OBJS) tests/test-%.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OUT)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ -MMD -MF $@.d $<

$(OUT):
	@mkdir -p $@

doc:
	@doxygen

clean:
	$(RM) $(EXEC) $(OBJS) $(deps)
	@rm -rf $(OUT)

distclean: clean
	rm -rf html
	make -C $(HTTP_PARSER_PATH) clean

-include $(deps)
