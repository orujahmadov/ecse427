################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../disk_emu.c \
../fuse_wrappers.c \
../sfs_api.c \
../sfs_test.c \
../sfs_test2.c 

OBJS += \
./disk_emu.o \
./fuse_wrappers.o \
./sfs_api.o \
./sfs_test.o \
./sfs_test2.o 

C_DEPS += \
./disk_emu.d \
./fuse_wrappers.d \
./sfs_api.d \
./sfs_test.d \
./sfs_test2.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


