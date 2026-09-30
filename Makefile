CXX ?= c++
CXXSTD ?= c++17
JUDGE_PARALLEL ?= 4
WARNINGS := -Wall -Wextra -Wshadow -Werror
SOURCES := $(wildcard src/*.h) $(wildcard src/shapes/*.h)
TESTS := tests/all.cpp tests/harness.h $(wildcard tests/*.inc)

.PHONY: all check amalgamate amalgamation-check test coverage e2e standards hostile budget bench examples codes version transcript mutants sanitize fuzz judge pin clean

all: eolymp.h eolymp-shapes.h

eolymp.h: $(SOURCES) tools/amalgamate.py
	python3 tools/amalgamate.py

eolymp-shapes.h: eolymp.h
	@test -f $@ || python3 tools/amalgamate.py

amalgamate:
	python3 tools/amalgamate.py

amalgamation-check:
	python3 tools/amalgamate.py --check

STANDARDS := $(filter-out $(CXXSTD),c++17 c++20 c++23)
OPTIMIZED := $(CXXSTD) c++23

build/tests-%: OPTIMIZATION = $(if $(filter $*,$(OPTIMIZED)),-O2,-O0)

build/tests-%: eolymp.h eolymp-shapes.h $(TESTS)
	@mkdir -p build
	$(CXX) -std=$* $(OPTIMIZATION) $(WARNINGS) -DEOLYMP_TESTING -o $@ tests/all.cpp

test: build/tests-$(CXXSTD)
	./build/tests-$(CXXSTD)

coverage: eolymp.h eolymp-shapes.h
	python3 tools/coverage.py

e2e: eolymp.h eolymp-shapes.h
	sh tests/e2e/run.sh

standards: $(STANDARDS:%=standard-%)

standard-%: build/tests-%
	@echo "standards: $*"
	@./build/tests-$*

.PRECIOUS: build/tests-%

hostile: eolymp.h eolymp-shapes.h
	sh tests/hostile/run.sh

budget: eolymp.h eolymp-shapes.h
	python3 tools/budget.py

bench: eolymp.h eolymp-shapes.h
	python3 tools/bench.py $(BASE)

examples: eolymp.h eolymp-shapes.h
	python3 tools/examples.py

codes:
	python3 tools/codes.py

version:
	python3 tools/version.py $(BASE)

transcript: eolymp.h eolymp-shapes.h
	sh tests/windows/build.sh build/transcript
	sh tests/windows/transcript.sh build/transcript > build/transcript.txt

SANITIZERS := -fsanitize=address,undefined -fno-sanitize-recover=all

SANITIZED := ASAN_OPTIONS=exitcode=86:halt_on_error=1 UBSAN_OPTIONS=exitcode=86:halt_on_error=1

sanitize: eolymp.h eolymp-shapes.h $(TESTS)
	@mkdir -p build
	$(CXX) -std=$(CXXSTD) -O0 -g $(WARNINGS) $(SANITIZERS) -DEOLYMP_TESTING -o build/tests-sanitized tests/all.cpp
	$(SANITIZED) ./build/tests-sanitized
	$(SANITIZED) E2E_BUILD="$(CURDIR)/build/e2e-sanitized" CXX="$(CXX) $(SANITIZERS)" sh tests/e2e/run.sh

FUZZ_CXX ?= clang++
FUZZ_SECONDS ?= 45
FUZZERS := $(patsubst tests/fuzz/%.cpp,%,$(wildcard tests/fuzz/*_fuzz.cpp))
FUZZ_FLAGS := -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -DEOLYMP_TESTING

build/fuzz/%: tests/fuzz/%.cpp tests/fuzz/fuzz.h eolymp.h eolymp-shapes.h
	@mkdir -p build/fuzz
	$(FUZZ_CXX) $(FUZZ_FLAGS) -o $@ $<

fuzz: $(addprefix fuzz-,$(or $(FUZZER),$(FUZZERS)))

fuzz-%: build/fuzz/%
	@mkdir -p build/fuzz/corpus/$* build/fuzz/crashes
	@echo "fuzz: $* for $(FUZZ_SECONDS) s"
	./build/fuzz/$* -max_total_time=$(FUZZ_SECONDS) -timeout=10 -rss_limit_mb=2048 -close_fd_mask=3 \
		-artifact_prefix=build/fuzz/crashes/$*- -print_final_stats=1 build/fuzz/corpus/$*

.PRECIOUS: build/fuzz/%

mutants: eolymp.h eolymp-shapes.h
	@mkdir -p build
	python3 tools/mutants.py

check: amalgamation-check
	$(MAKE) test coverage standards e2e hostile examples codes
	$(MAKE) budget

build/eo-judge: $(wildcard judge/*.go) judge/go.mod judge/include/eolymp.h judge/include/eolymp-shapes.h \
		$(wildcard judge/templates/*/*)
	@mkdir -p build
	cd judge && CGO_ENABLED=0 go build -trimpath -o ../build/eo-judge .

judge: eolymp.h eolymp-shapes.h
	cd judge && unformatted=$$(gofmt -l .) && \
		if [ -n "$$unformatted" ]; then echo "judge: gofmt would change $$unformatted" >&2; exit 1; fi && \
		go vet ./... && go test -count=1 -parallel $(JUDGE_PARALLEL) ./...

pin:
	cd judge && AGENT_REPO=$(or $(AGENT_REPO),../../agent) go test -count=1 -run TestTheCopiedPointsParser -v ./...

clean:
	rm -rf build
	rm -f *.gcov
