set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu='arm926ej-s' -mfloat-abi=soft -O3 -Wall -Wno-char-subscripts -Wno-write-strings -fno-stack-protector")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu='arm926ej-s' -mfloat-abi=soft -mthumb -O0 -Wall -x assembler-with-cpp -fmessage-length=0")
