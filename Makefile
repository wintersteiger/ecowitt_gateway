# Copyright (c) Christoph M. Wintersteiger
# Licensed under the MIT License.

WLMCD_ROOT=../wlmcd

# CXX=clang++-8
# CXXFLAGS+=-O1 -g -fsanitize=address -fno-omit-frame-pointer -D_GLIBCXX_DEBUG
# LDFLAGS+=-g -fsanitize=address

CXXFLAGS+=-g -MMD -MP -I ${WLMCD_ROOT} -Wno-psabi

%.o: %.cpp
	${CXX} ${CXXFLAGS} $< -c -o $@

all: gateway

SRC = gateway.cpp ecowitt_wh51.cpp ecowitt_wh51_ui.cpp pqarchive.cpp
OBJ = $(subst .cpp,.o,$(SRC))

LDFLAGS_STATIC=${LDFLAGS} ${WLMCD_ROOT}/libwlmcd-ui.a ${WLMCD_ROOT}/libwlmcd-dev.a  -lpthread -lrt -lncurses -lcrypto -lgpiod -lpq -lpqxx

gateway: ${OBJ}  ${WLMCD_ROOT}/libwlmcd-ui.a ${WLMCD_ROOT}/libwlmcd-dev.a
	${CXX} -o $@ ${OBJ} ${LDFLAGS_STATIC}

clean:
	rm -rf *.o gateway

-include *.d
