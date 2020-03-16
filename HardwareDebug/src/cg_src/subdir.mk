################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/cg_src/r_cg_cgc.c \
../src/cg_src/r_cg_cgc_user.c \
../src/cg_src/r_cg_it.c \
../src/cg_src/r_cg_it_user.c \
../src/cg_src/r_cg_main.c \
../src/cg_src/r_cg_port.c \
../src/cg_src/r_cg_port_user.c \
../src/cg_src/r_cg_sau.c \
../src/cg_src/r_cg_sau_user.c \
../src/cg_src/r_cg_systeminit.c \
../src/cg_src/r_cg_wdt.c \
../src/cg_src/r_cg_wdt_user.c \
../src/cg_src/u_data_flash2.c \
../src/cg_src/u_keypad.c \
../src/cg_src/u_modbus2.c 

COMPILER_OBJS += \
src/cg_src/r_cg_cgc.obj \
src/cg_src/r_cg_cgc_user.obj \
src/cg_src/r_cg_it.obj \
src/cg_src/r_cg_it_user.obj \
src/cg_src/r_cg_main.obj \
src/cg_src/r_cg_port.obj \
src/cg_src/r_cg_port_user.obj \
src/cg_src/r_cg_sau.obj \
src/cg_src/r_cg_sau_user.obj \
src/cg_src/r_cg_systeminit.obj \
src/cg_src/r_cg_wdt.obj \
src/cg_src/r_cg_wdt_user.obj \
src/cg_src/u_data_flash2.obj \
src/cg_src/u_keypad.obj \
src/cg_src/u_modbus2.obj 

C_DEPS += \
src/cg_src/r_cg_cgc.d \
src/cg_src/r_cg_cgc_user.d \
src/cg_src/r_cg_it.d \
src/cg_src/r_cg_it_user.d \
src/cg_src/r_cg_main.d \
src/cg_src/r_cg_port.d \
src/cg_src/r_cg_port_user.d \
src/cg_src/r_cg_sau.d \
src/cg_src/r_cg_sau_user.d \
src/cg_src/r_cg_systeminit.d \
src/cg_src/r_cg_wdt.d \
src/cg_src/r_cg_wdt_user.d \
src/cg_src/u_data_flash2.d \
src/cg_src/u_keypad.d \
src/cg_src/u_modbus2.d 

# Each subdirectory must supply rules for building sources it contributes
src/cg_src/%.obj: ../src/cg_src/%.c 
	@echo 'Scanning and building file: $<'
	@echo 'Invoking: Scanner and Compiler'
	@echo src\cg_src\cDepSubCommand.tmp=
	@sed -e "s/^/ /" "src\cg_src\cDepSubCommand.tmp"
	ccrl -subcommand="src\cg_src\cDepSubCommand.tmp" -o "$(@:%.obj=%.d)"  -MT="$(@:%.obj=%.obj)"  -MT="$(@:%.obj=%.d)" -msg_lang=english "$<"
	@echo src\cg_src\cSubCommand.tmp=
	@sed -e "s/^/ /" "src\cg_src\cSubCommand.tmp"
	ccrl -subcommand="src\cg_src\cSubCommand.tmp" -msg_lang=english -o "$(@:%.d=%.obj)" "$<"
	@echo 'Finished Scanning and building: $<'
	@echo.

