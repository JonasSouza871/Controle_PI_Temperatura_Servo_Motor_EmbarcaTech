# Versão mínima do CMake necessária
cmake_minimum_required(VERSION 3.13)

# Definir padrões de linguagem
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Exportar comandos de compilação para IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Definir tipo de placa (Pico W - com WiFi)
set(PICO_BOARD pico_w CACHE STRING "Tipo da placa")

# Incluir o SDK do Raspberry Pi Pico (deve vir antes do project())
include(pico_sdk_import.cmake)

# Nome do projeto
project(Controle_PI_Servo_Temperatura C CXX ASM)

# Inicializar o SDK do Raspberry Pi Pico
pico_sdk_init()

# Criar executável principal (apenas com o main.c)
add_executable(Controle_PI_Servo_Temperatura 
    main.c
    lib/aht20.c
    lib/pico_http_server.c
)

# Definir nome e versão do programa
pico_set_program_name(Controle_PI_Servo_Temperatura "Controle_PI_Servo_Temperatura")
pico_set_program_version(Controle_PI_Servo_Temperatura "0.1")

pico_enable_stdio_uart(Controle_PI_Servo_Temperatura 1) # Habilitar saída via UART (comunicação serial)
pico_enable_stdio_usb(Controle_PI_Servo_Temperatura 1) # Habilitar saída via USB (para debugging)

# Incluir diretórios de header files para o executável
target_include_directories(Controle_PI_Servo_Temperatura PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib
)

target_link_libraries(Controle_PI_Servo_Temperatura
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_pwm
    pico_cyw43_arch_lwip_threadsafe_background
    )

# Gerar arquivos de saída adicionais (.uf2, .hex, etc.)
pico_add_extra_outputs(Controle_PI_Servo_Temperatura)
