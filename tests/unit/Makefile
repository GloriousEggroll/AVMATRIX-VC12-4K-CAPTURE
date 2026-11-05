# Stage 7.5: tests for the driver's pure code, built in userspace under
# ASan and UBSan. No card or kernel headers needed.
#
#   make -C tests/unit check
#
# check also confirms the slack test can fail: it rebuilds test_calc with
# HWS_ZC_SLACK one byte below the least that fits and expects a failure.

SRC := ../../src
# GCC needs libasan and libubsan installed; clang's runtime comes with compiler-rt
CC ?= cc
SAN := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
CFLAGS ?= -O1 -g
CFLAGS += -std=gnu11 $(SAN) -I$(SRC)
STRICT := -Wall -Wextra -Werror
LDFLAGS += $(SAN)

all: test_calc test_scale

test_calc: test_calc.c $(SRC)/hws_calc.h
	$(CC) $(CFLAGS) $(STRICT) -o $@ $< $(LDFLAGS)

# hws_scale.c is the vendor's code: build it, but don't hold its warnings to
# -Werror. Its fixed-size scalers copy pixels through int pointers at odd
# byte offsets (FHD_To_HD_Scaler and others): undefined in C, harmless on
# x86-64, a fault on CPUs without misaligned loads. Recorded in results.md
# (7.5); the alignment check is off for this file only.
hws_scale.o: $(SRC)/hws_scale.c $(SRC)/hws_scale.h
	$(CC) $(CFLAGS) -fno-sanitize=alignment -w -c -o $@ $<

test_scale: test_scale.c hws_scale.o $(SRC)/hws_scale.h
	$(CC) $(CFLAGS) $(STRICT) -o $@ test_scale.c hws_scale.o $(LDFLAGS)

test_calc_slack: test_calc.c $(SRC)/hws_calc.h
	$(CC) $(CFLAGS) -DHWS_ZC_SLACK=4095 -o $@ $< $(LDFLAGS)

check: test_calc test_scale test_calc_slack
	./test_calc
	./test_scale
	@if ./test_calc_slack >/dev/null 2>&1; then \
		echo "test_calc passed with HWS_ZC_SLACK=4095: the slack check can't fail"; exit 1; \
	else echo "test_calc fails with HWS_ZC_SLACK=4095, as it should"; fi

clean:
	rm -f test_calc test_scale test_calc_slack hws_scale.o

.PHONY: all check clean
