MODULES_DIR = /var/lib/znc/modules

all: uptimelog.so
	

uptimelog.so: uptimelog.cpp
	znc-buildmod $?

copy: uptimelog.so
	cp $< $(MODULES_DIR)

.PHONY: clean
clean:
	rm -f uptimelog.so