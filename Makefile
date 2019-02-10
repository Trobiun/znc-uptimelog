MODULES_DIR = /var/lib/znc/modules
export INCLUDES=-lcurl -Ilib
export CXXFLAGS=-Wno-psabi

all: uptimelog.so
	

uptimelog.so: uptimelog.cpp
	znc-buildmod $?

copy: uptimelog.so
	cp $< $(MODULES_DIR)

.PHONY: clean
clean:
	rm -f uptimelog.so
