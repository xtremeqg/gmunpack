CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -fPIC -fno-rtti -std=c++14 -O3 -ffunction-sections -fdata-sections -flto -s
LDFLAGS = -Wl,-gc-sections -flto -s
TARGETS = gmunpack

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -fr $(TARGETS) obj

obj:
	mkdir -p $@

gmunpack: $(patsubst src/%.cpp,obj/%.o,$(wildcard src/*.cpp))
	$(CXX) $(LDFLAGS) -o $@ $^

obj/%.o: src/%.cpp | obj
	$(CXX) $(CXXFLAGS) -c $< -o $@
