.PHONY: csim cosim hw_emu hw hls clean exe bitstream

APP ?= kernel
SRC ?= src
PLATFORM ?= xilinx_u50_gen3x16_xdma_5_202210_1

KERNEL_SRCS ?= device_kernel.cpp
KERNEL_NAME ?= DeviceChainKernel
HOST_SRCS ?= host_data_io.cpp main.cpp memory_scheduler.cpp
INPUT ?= input.txt
OUTPUT ?= output.txt
GOLDEN ?= golden.txt
HOST_ARGS ?= $(INPUT) $(OUTPUT)

SRC ?= src
OBJ ?= obj/$(PLATFORM)
BIN ?= bin/$(PLATFORM)
BIT ?= bit/$(PLATFORM)
TMP ?= tmp/$(PLATFORM)

TAPACXX ?= tapa -w $(TMP) g++ --
TAPA_COMPILE ?= tapa -w $(TMP) compile
CLCXX ?= v++

TAPA_CFLAGS += -g -O2 -I$(SRC)
TAPA_COMPILE_OPT += --platform $(PLATFORM) --top $(KERNEL_NAME)
CLCXX_OPT += --platform $(PLATFORM) --kernel $(KERNEL_NAME) --report_level 2 --link

csim: $(BIN)/$(APP)
	$(BIN)/$(APP) "" $(HOST_ARGS)

cosim: $(BIN)/$(APP) $(BIT)/$(APP).xo
	$(BIN)/$(APP) $(BIT)/$(APP).xo $(HOST_ARGS)

hw_emu: $(BIN)/$(APP) $(BIT)/$(APP).hw_emu.xclbin
	$(BIN)/$(APP) $(BIT)/$(APP).hw_emu.xclbin $(HOST_ARGS)

hw: $(BIN)/$(APP) $(BIT)/$(APP).xclbin
	$(BIN)/$(APP) $(BIT)/$(APP).xclbin $(HOST_ARGS)

hls: $(BIT)/$(APP).xo

clean:
	rm -rf $(OBJ) $(BIN) $(BIT) $(TMP) *.log *.jou output.txt

exe: $(BIN)/$(APP)

bitstream: $(BIT)/$(APP).xclbin

###################### targets  ######################

$(BIN)/$(APP): $(addprefix $(OBJ)/, $(HOST_SRCS:.cpp=.o)) $(addprefix $(OBJ)/, $(KERNEL_SRCS:.cpp=.o))
	@mkdir -p $(BIN)
	$(TAPACXX) $(TAPA_CFLAGS) -o $@ $^

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(OBJ)
	$(TAPACXX) $(TAPA_CFLAGS) -c -o $@ $<

$(BIT)/$(APP).xo: $(addprefix $(SRC)/, $(KERNEL_SRCS))
	@mkdir -p $(BIT)
	$(TAPA_COMPILE) $(TAPA_COMPILE_OPT) -o $@ -f $^ 

$(BIT)/$(APP).hw_emu.xclbin: $(BIT)/$(APP).xo
	@mkdir -p $(BIT)
	$(CLCXX) -t hw_emu --temp_dir $(TMP) $(CLCXX_OPT) -o $@ $^

$(BIT)/$(APP).xclbin: $(BIT)/$(APP).xo
	@mkdir -p $(BIT)
	$(CLCXX) -t hw --temp_dir $(TMP) $(CLCXX_OPT) -o $@ $^
