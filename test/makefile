temp ?= /tmp/build/binarywheel
out ?= /tmp/binarywheel
cov := --coverage -std=c++17 -O0 -fno-inline -fno-inline-small-functions -fno-default-inline
GCOV ?= gcov

all: $(out)/test

.PHONY: test cov

test: $(out)/test
	cd $(out) && ./test
	npm run -s test

cov: $(out)/cov/index.html

clean:
	rm -rf $(out) $(temp)

$(temp)/%.cpp.o: %.cpp $(temp)/testtypes.hpp makefile
	@mkdir -p $(@D)
	$(CXX) -MMD -MP -MF $@.d -g $(cov) -I.. -I$(temp) -c $< -o $@

$(out)/test: $(temp)/test.cpp.o makefile
	@mkdir -p $(@D)
	$(CXX) $(cov) $< -o $@

$(temp)/testtypes.hpp: testtypes.coffee ../cpp.coffee makefile
	@mkdir -p $(@D)
	../bw-gen-cpp $< -o $@

$(out)/cpp.info: $(out)/test makefile
	@rm -f $@
	@lcov -q --gcov-tool=$(GCOV) -i -c -b .. -d $(temp)/ -o $@
	$(out)/test
	@lcov -q --gcov-tool=$(GCOV) -c -b .. -d $(temp)/ -o $@
	@lcov -q --gcov-tool=$(GCOV) -e $@ '*/binarywheel.hpp' -o $@

$(out)/coffee.info: *.coffee ../*.coffee
	npm run -s test
	npm run -s cov >$@

$(out)/cov/index.html: $(out)/cpp.info $(out)/coffee.info
	@rm -rf $(out)/cov
	genhtml -q --no-sort --demangle-cpp --num-spaces 4 $^ --output-directory $(out)/cov

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
-include $(call rwildcard, $(temp)/, *.o.d)
