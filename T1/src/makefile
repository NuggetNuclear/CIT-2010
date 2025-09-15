# Makefile para compilar e interactuar con el proyecto de T1
# Targets disponibles: run, clean, debug, clean-fifos

# comandos:
#   make run         - compila y ejecuta la central (usa Ctrl+C para terminar)
#   make clean       - elimina binarios y objetos compilados
#   make debug       - compila con símbolos de depuración y sanitizers
#   make clean-fifos - elimina FIFOs temporales en /tmp

# ---- Herramientas y flags ----
CXX      := g++
CXXSTD   := -std=c++17
WARNINGS := -Wall -Wextra -Wpedantic
OPT      := -O2

ifeq ($(DEBUG),1)
  OPT := -O0 -g3
endif

SAN_FLAGS :=
ifeq ($(SAN),1)
  SAN_FLAGS := -fsanitize=address,undefined
endif

CXXFLAGS := $(CXXSTD) $(WARNINGS) $(OPT) $(SAN_FLAGS)
LDFLAGS  := $(SAN_FLAGS)

# ---- Directorios ----
BUILD_DIR := build
BIN_DIR   := bin

# ---- Fuentes ----
SOURCES := central.cpp client.cpp moderator.cpp
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# ---- Phony ----
.PHONY: run clean debug clean-fifos all

# Compilación de binarios (implícita antes de run)
all: $(BIN_DIR)/central $(BIN_DIR)/client $(BIN_DIR)/moderator

# Ejecutar la central (compila antes). Ctrl+C para terminar.
run: all
	@echo "Iniciando central, recuerda ejecutar clientes."
	@PATH="$(abspath $(BIN_DIR)):$$PATH" "$(BIN_DIR)/central"

# Limpiar artefactos de compilación
clean:
	@rm -rf "$(BUILD_DIR)" "$(BIN_DIR)"

# Limpiar FIFOs temporales (ajusta patrón si cambias los nombres)
clean-fifos:
	@echo "Eliminando FIFOs /tmp/orch_*.fifo (ignora si no existen)..."
	@rm -f /tmp/orch_*.fifo

# Compilación con símbolos + sanitizers
debug:
	@$(MAKE) DEBUG=1 SAN=1 all

# ---- Reglas de enlace ----
$(BIN_DIR)/central: $(BUILD_DIR)/central.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/client: $(BUILD_DIR)/client.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/moderator: $(BUILD_DIR)/moderator.o | $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS)

# ---- Regla de compilación ----
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- Crear directorios ----
$(BUILD_DIR) $(BIN_DIR):
	@mkdir -p $@