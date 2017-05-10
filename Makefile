ifeq ($(INSTALL_PREFIX),)
INSTALL_PREFIX=/usr
endif

PREFIX=$(DESTDIR)$(INSTALL_PREFIX)


all: src/xpybind

src/xpybind:
	$(MAKE) -C src xpybind

clean:
	$(MAKE) -C src clean
	find . \( -name \*~ -o -name \*.pyc -o -name '*.o' -o -name '#*#' \) \
	-exec rm {} ';'
    
install:
	$(MAKE) -C src install
	$(MAKE) -C man install
