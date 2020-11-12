################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
FreeRTOS_Source/%.obj: ../FreeRTOS_Source/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: MSP430 Compiler'
	"/Applications/ti/ccs1000/ccs/tools/compiler/ti-cgt-msp430_20.2.0.LTS/bin/cl430" -vmspx --data_model=large --near_data=none -O0 --use_hw_mpy=F5 --include_path="/Applications/ti/ccs1000/ccs/ccs_base/msp430/include" --include_path="/Users/daniel/projects/Intermittent-Distributed/ThirdParty" --include_path="/Users/daniel/projects/Intermittent-Distributed" --include_path="/Users/daniel/projects/Intermittent-Distributed/FreeRTOS_Source/include" --include_path="/Users/daniel/projects/Intermittent-Distributed/FreeRTOS_Source/portable/CCS/MSP430X" --include_path="/Applications/ti/ccs1000/ccs/tools/compiler/ti-cgt-msp430_20.2.0.LTS/include" --include_path="/Users/daniel/projects/Intermittent-Distributed/driverlib/MSP430FR5xx_6xx" --include_path="/Users/daniel/projects/Intermittent-Distributed/DataManager" --include_path="/Users/daniel/projects/Intermittent-Distributed/ExpTests" --include_path="/Users/daniel/projects/Intermittent-Distributed/RecoveryHandler" --include_path="/Users/daniel/projects/Intermittent-Distributed/TestArangement" --include_path="/Users/daniel/projects/Intermittent-Distributed/Tools" --include_path="/Users/daniel/projects/Intermittent-Distributed/Connectivity" --include_path="/Users/daniel/projects/Intermittent-Distributed/Peripheral" --define=__MSP430FR5994__ -g --c11 --printf_support=full --diag_warning=225 --diag_wrap=off --display_error_number --enum_type=packed --abi=eabi --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU40 --preproc_with_compile --preproc_dependency="FreeRTOS_Source/$(basename $(<F)).d_raw" --obj_directory="FreeRTOS_Source" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


