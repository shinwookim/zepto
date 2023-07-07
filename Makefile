SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

OUT	:= zepto
EXE := $(BIN_DIR)/$(OUT)
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)


CFLAGS   := -Werror -g
LDLIBS   := -lm

.PHONY: all clean run

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC)  $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC)  $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

run: $(EXE)
	$(EXE)

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

-include $(OBJ:.o=.d)