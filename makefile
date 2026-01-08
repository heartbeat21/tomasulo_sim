# 编译器和标准
CXX       = g++
CXXFLAGS_RELEASE = -std=c++17 -O2 -Wall -Wextra -g
CXXFLAGS_DEBUG   = -std=c++17 -O0 -g -Wall -Wextra -DDEBUG
INCLUDES  = -Isrc

# 目录配置
SRCDIR = src
BUILDDIR = build

# 自动发现源文件
SRCS := $(wildcard $(SRCDIR)/*.cpp)

CXXFLAGS ?= $(CXXFLAGS_RELEASE)

# 自动生成 build/ 下的 .o 文件路径
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# 分组（注意：现在对象文件在 build/ 下）
COMMON_OBJS   := $(addprefix $(BUILDDIR)/, instruction.o loader.o decoder.o)
TRANSLATOR_OBJS := $(addprefix $(BUILDDIR)/, translator.o)
TOMASULO_OBJS := $(addprefix $(BUILDDIR)/, main.o tomasulo_sim.o)

# 可执行文件也放在 build/
TRANSLATOR = $(BUILDDIR)/translator
TOMASULO   = $(BUILDDIR)/tomasulo

# 默认目标
all: $(TRANSLATOR) $(TOMASULO)

debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: $(TOMASULO)

# 确保 build/ 目录存在
$(BUILDDIR):
	mkdir -p $@

# 构建 translator
$(TRANSLATOR): $(TRANSLATOR_OBJS) $(COMMON_OBJS)  | $(BUILDDIR)
	$(CXX) $^ -o $@

# 构建 tomasulo
$(TOMASULO): $(COMMON_OBJS) $(TOMASULO_OBJS) | $(BUILDDIR)
	$(CXX) $^ -o $@

# 核心规则：编译 src/%.cpp → build/%.o
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -rf $(BUILDDIR)

# 重新构建
rebuild: clean all
rebuild-debug: clean debug

.PHONY: all debug clean rebuild rebuild-debugmake