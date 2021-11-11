all: lib server client drive

.PHONY: server
server: lib
	@echo "[LIB]"
	@make -C server

.PHONY: client

client: lib
	@echo "[CLIENT]"
	@make -C client

.PHONY: lib
lib:
	@echo "[SERVER]"
	@make -C lib

.PHONY : drive
drive:
	@echo "[drive]"
	@make -C drive

.PHONY: clean
clean:
	@for d in lib server client; do \
		make -C $$d clean ; \
	done
#	@rm -f cscope.out tags

#.PHONY: cscope
#cscope:
#	cscope -b -R
#	ctags -R *

.PHONY: autobuild
autobuild:
	monex.py server/*.[ch] include/*.[ch] lib/*.[ch] client/*.[ch] -c "make clean; make"

.PHONY: lc
lc:
	find . -iname "*.[ch]" | xargs wc -l
