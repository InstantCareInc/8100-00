################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'MSP430 Compiler - building file: "$<"'
	"C:/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/bin/cl430" -vmsp -Ooff --use_hw_mpy=16 --include_path="C:/ti/ccs2100/ccs/ccs_base/msp430/include" --include_path="C:/Users/Austin/Medical Alert/Instant Care Engineering - Lifeline/Wireless Link/Firmware/8100-00/8100-00" --include_path="C:/ti/ccs2100/ccs/tools/compiler/ti-cgt-msp430_21.6.2.LTS/include" --advice:power=all --define=__MSP430F1611__ -g --printf_support=minimal --diag_warning=225 --diag_wrap=off --display_error_number --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


