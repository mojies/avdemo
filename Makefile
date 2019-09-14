
all:
	make -C FFDeamon
	make -C cli
	echo "build done!"

clean:
	make clean -C FFDeamon
	make clean -C cli

install:
	make install -C FFDeamon
	make install -C cli
