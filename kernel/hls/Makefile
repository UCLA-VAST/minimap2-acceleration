.PHONY: csim cosim hw hls clean exe bitstream check-afi-status check-aws-bucket

APP ?= kernel
SDA_VER ?= 2018.3
XILINX_SDX ?= /opt/tools/xilinx/SDx/$(SDA_VER)
XILINX_XRT ?= /opt/xilinx/xrt
XDEVICE ?= xilinx:vcu1525:xdma:201830.1

CSIM_XCLBIN ?= $(APP)-csim.xclbin
COSIM_XCLBIN ?= $(APP)-cosim.xclbin
HW_XCLBIN ?= $(APP)-hw.xclbin

KERNEL_SRCS ?= device_kernel.cpp
KERNEL_NAME ?= device_chain_kernel
HOST_SRCS ?= common.cpp device_kernel_wrapper.cpp \
			 host_data_io.cpp host_kernel.cpp \
			 main.cpp memory_scheduler.cpp
HOST_ARGS ?=
HOST_BIN ?= $(APP)

SRC ?= src
OBJ ?= obj/$(SDA_VER)/$(word 2,$(subst :, ,$(XDEVICE)))
BIN ?= bin/$(SDA_VER)/$(word 2,$(subst :, ,$(XDEVICE)))
BIT ?= bit/$(SDA_VER)/$(word 2,$(subst :, ,$(XDEVICE)))
RPT ?= rpt/$(SDA_VER)/$(word 2,$(subst :, ,$(XDEVICE)))
TMP ?= tmp/$(SDA_VER)/$(word 2,$(subst :, ,$(XDEVICE)))

AWS_AFI_DIR ?= afis
AWS_AFI_LOG ?= logs
CXX ?= g++
CLCXX ?= xocc

HOST_CFLAGS += -std=c++0x -g -O2 -Wall -DFPGA_DEVICE -DC_KERNEL
HOST_CFLAGS += -I$(XILINX_XRT)/include
HOST_CFLAGS += -I$(XILINX_SDX)/runtime/include/1_2
HOST_CFLAGS += -I$(subst SDx,Vivado,$(XILINX_SDX))/include
HOST_CFLAGS += -I$(subst SDx,Vivado_HLS,$(XILINX_SDX))/include
HOST_LFLAGS += -L$(XILINX_XRT)/lib -lxilinxopencl -lrt
HOST_LFLAGS += -L$(XILINX_SDX)/runtime/lib/x86_64 -lxilinxopencl -lrt

ifeq ($(XDEVICE),"xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0")
	SDA_VER = 2017.1
endif

ifeq ($(SDA_VER),2017.1)
	# 2017.1
	PLATFORM := $(XDEVICE)
	HOST_CFLAGS += -DTARGET_DEVICE=\"$(PLATFORM)\"
else
ifeq ($(SDA_VER),2017.2)
	# 2017.2
	PLATFORM := $(XDEVICE)
	HOST_CFLAGS += -DTARGET_DEVICE=\"$(subst :,_,$(subst .,_,$(PLATFORM)))\"
else
	# 2017.4 or 2018.2
	PLATFORM := $(subst :,_,$(subst .,_,$(XDEVICE)))
	HOST_CFLAGS += -DTARGET_DEVICE=\"$(PLATFORM)\"
endif
endif

CLCXX_OPT += $(CLCXX_OPT_LEVEL) $(DEVICE_REPO_OPT) --platform $(PLATFORM) $(KERNEL_DEFS) $(KERNEL_INCS)
CLCXX_OPT += --kernel $(KERNEL_NAME)
CLCXX_OPT += --xp prop:kernel.$(KERNEL_NAME).kernel_flags="-std=c++0x"
CLCXX_OPT += --sp device_chain_kernel_1.m_axi_gmem_0:bank0 --sp device_chain_kernel_1.m_axi_gmem_1:bank1
CLCXX_OPT += -s -g
CLCXX_CSIM_OPT = -t sw_emu --temp_dir $(abspath $(TMP)/$(CSIM_XCLBIN:.xclbin=))
CLCXX_COSIM_OPT = -t hw_emu --temp_dir $(abspath $(TMP)/$(COSIM_XCLBIN:.xclbin=))
CLCXX_HW_OPT = -t hw --temp_dir $(abspath $(TMP)/$(HW_XCLBIN:.xclbin=))

############################## phony targets ##############################

csim-target: $(BIN)/$(HOST_BIN) $(BIT)/$(CSIM_XCLBIN) $(BIN)/emconfig.json
	@rm -f sdaccel_profile_summary.{csv,html}

csim: $(BIN)/$(HOST_BIN) $(BIT)/$(CSIM_XCLBIN) $(BIN)/emconfig.json
	ulimit -s unlimited;. $(XILINX_SDX)/settings64.sh;XCL_EMULATION_MODE=sw_emu $(BIN)/$(HOST_BIN) $(BIT)/$(CSIM_XCLBIN) $(HOST_ARGS)
	@rm -f sdaccel_profile_summary.{csv,html}

cosim-target: $(BIN)/$(HOST_BIN) $(BIT)/$(COSIM_XCLBIN) $(BIN)/emconfig.json
	@rm -f sdaccel_profile_summary.{csv,html}

cosim: $(BIN)/$(HOST_BIN) $(BIT)/$(COSIM_XCLBIN) $(BIN)/emconfig.json
	. $(XILINX_SDX)/settings64.sh;XCL_EMULATION_MODE=hw_emu $(BIN)/$(HOST_BIN) $(BIT)/$(COSIM_XCLBIN) $(HOST_ARGS)
	@rm -f sdaccel_profile_summary.{csv,html} ${PLATFORM}-*-$(APP)-cosim_simulate.log

ifneq (,$(findstring aws-vu9p-f1,$(XDEVICE)))
bitstream: $(BIT)/$(HW_XCLBIN:.xclbin=.awsxclbin)

hw: $(BIN)/$(HOST_BIN) $(BIT)/$(HW_XCLBIN:.xclbin=.awsxclbin)
	. $(XILINX_SDX)/settings64.sh;$^ $(HOST_ARGS)
	@rm -f sdaccel_profile_summary.{csv,html}

else
bitstream: $(BIT)/$(HW_XCLBIN)

hw: $(BIN)/$(HOST_BIN) $(BIT)/$(HW_XCLBIN)
	. $(XILINX_SDX)/settings64.sh;$^ $(HOST_ARGS)
	@rm -f sdaccel_profile_summary.{csv,html}
endif

hls: $(OBJ)/$(HW_XCLBIN:.xclbin=.xo)

exe: $(BIN)/$(HOST_BIN)

check-afi-status:
	@echo -n 'AFI state: ';aws ec2 describe-fpga-images --fpga-image-ids $$(jq -r '.FpgaImageId' $(BIT)/$(HW_XCLBIN:.xclbin=.afi))|jq '.FpgaImages[0].State.Code' -r

check-aws-bucket:
ifndef AWS_BUCKET
	$(error AWS_BUCKET must be set to an available AWS S3 bucket)
endif

clean:
	rm -rf $(BIN) $(BIT) $(RPT) $(OBJ) $(TMP) .Xil sdaccel_profile_summary.{csv,html}
	rmdir -p $(BIN) $(BIT) $(RPT) $(OBJ) $(TMP) --ignore-fail-on-non-empty 2>/dev/null || true

############################## helpers ##############################

WITH_SDACCEL = echo "$(1)";TMP=$$(mktemp -d /tmp/sdaccel-XXXXXXXXXX);cd $${TMP};. $(XILINX_SDX)/settings64.sh;$(1);RC=$$?;rm -rf $${TMP};exit $${RC}

############################## bin ##############################

$(BIN)/$(HOST_BIN): $(HOST_SRCS:%.cpp=$(OBJ)/%.o)
	@mkdir -p $(BIN)
	@$(call WITH_SDACCEL,$(CXX) $(abspath $^) $(HOST_LFLAGS) -o $(abspath $@))

$(BIN)/emconfig.json:
	@mkdir -p $(BIN)
	@echo "emconfigutil --platform $(PLATFORM) $(DEVICE_REPO_OPT) --od $(BIN)"
	@cd $(BIN);ln -sf /tmp/ .Xil;. $(XILINX_SDX)/settings64.sh;emconfigutil --platform $(PLATFORM) $(DEVICE_REPO_OPT);rm -f .Xil

#@$(call WITH_SDACCEL,emconfigutil --platform $(PLATFORM) $(DEVICE_REPO_OPT) --od $(abspath $(BIN)))

############################## obj ##############################

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(OBJ)
	@$(call WITH_SDACCEL,$(CXX) $(HOST_CFLAGS) -MM -MP -MT $@ -MF $(abspath $(@:.o=.d)) $(abspath $<))
	@$(call WITH_SDACCEL,$(CXX) $(HOST_CFLAGS) -c $(abspath $<) -o $(abspath $@))

$(OBJ)/$(CSIM_XCLBIN:.xclbin=.xo): $(SRC)/$(KERNEL_SRCS)
	@mkdir -p $(OBJ)
	@$(call WITH_SDACCEL,$(CLCXX) $(CLCXX_CSIM_OPT) $(CLCXX_OPT) -c -o $(abspath $@) $(abspath $<))

$(OBJ)/$(HW_XCLBIN:.xclbin=.xo): $(SRC)/$(KERNEL_SRCS)
	@mkdir -p $(OBJ)
	@$(call WITH_SDACCEL,$(CLCXX) $(CLCXX_HW_OPT) $(CLCXX_OPT) --log --log_dir $(abspath $(RPT)/$(HW_XCLBIN:.xclbin=)) --report system --report_dir $(abspath $(RPT)/$(HW_XCLBIN:.xclbin=)) -c -o $(abspath $@) $(abspath $<))

-include $(OBJ)/$(HOST_SRCS:%.cpp=%.d)

############################## bit ##############################

$(BIT)/$(CSIM_XCLBIN): $(OBJ)/$(CSIM_XCLBIN:.xclbin=.xo)
	@mkdir -p $(BIT)
	@$(call WITH_SDACCEL,$(CLCXX) $(CLCXX_CSIM_OPT) $(CLCXX_OPT) -l -o $(abspath $@) $(abspath $<))

$(BIT)/$(COSIM_XCLBIN): $(OBJ)/$(HW_XCLBIN:.xclbin=.xo)
	@mkdir -p $(BIT)
	@$(call WITH_SDACCEL,$(CLCXX) $(CLCXX_COSIM_OPT) $(CLCXX_OPT) -l -o $(abspath $@) $(abspath $<))

$(BIT)/$(HW_XCLBIN): $(OBJ)/$(HW_XCLBIN:.xclbin=.xo)
	@mkdir -p $(BIT)
	@$(call WITH_SDACCEL,$(CLCXX) $(CLCXX_HW_OPT) $(CLCXX_OPT) --log --log_dir $(abspath $(RPT)/$(HW_XCLBIN:.xclbin=)) --report system --report_dir $(abspath $(RPT)/$(HW_XCLBIN:.xclbin=))	-l -o $(abspath $@) $(abspath $<))

$(BIT)/$(HW_XCLBIN:.xclbin=.awsxclbin): check-aws-bucket $(BIT)/$(HW_XCLBIN)
	@TMP=$$(mktemp -d);ln -rs ${BIT}/$(HW_XCLBIN) $${TMP};pushd $${TMP} >/dev/null;create-sdaccel-afi -xclbin=$(HW_XCLBIN) -o=$(HW_XCLBIN:.xclbin=) -s3_bucket=$(AWS_BUCKET) -s3_dcp_key=$(AWS_AFI_DIR) -s3_logs_key=$(AWS_AFI_LOG);popd >/dev/null;mv $${TMP}/$(HW_XCLBIN:.xclbin=.awsxclbin) $(BIT);mv $${TMP}/*afi_id.txt $(BIT)/$(HW_XCLBIN:.xclbin=.afi);rm -rf $${TMP}
