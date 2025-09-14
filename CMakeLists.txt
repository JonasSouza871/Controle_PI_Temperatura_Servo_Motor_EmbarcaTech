# Versão mínima do CMake necessária
cmake_minimum_required(VERSION 3.13)

# Definir padrões de linguagem
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Exportar comandos de compilação para IDEs

# Definir tipo de placa (Pico W - com WiFi)
set(PICO_BOARD pico_w CACHE STRING "Tipo da placa")

# ATENÇÃO: NÃO EDITAR AS LINHAS ABAIXO - necessárias para a extensão VS Code
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

# Versões dos componentes
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)

# Configuração específica para VS Code
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Incluir o SDK do Raspberry Pi Pico (deve vir antes do project())
include(pico_sdk_import.cmake)
project(Controle_PI_Servo_Temperatura C CXX ASM) # Nome do projeto
pico_sdk_init() # Inicializar o SDK do Raspberry Pi Pico

# Criar executável principal
add_executable(Controle_PI_Servo_Temperatura 
    Controle_PI_Servo_Temperatura.c
)

# Definir nome e versão do programa
pico_set_program_name(Controle_PI_Servo_Temperatura "Controle_PI_Servo_Temperatura")
pico_set_program_version(Controle_PI_Servo_Temperatura "0.1")

pico_enable_stdio_uart(Controle_PI_Servo_Temperatura 1)# Habilitar saída via UART (comunicação serial)
pico_enable_stdio_usb(Controle_PI_Servo_Temperatura 1) # Habilitar saída via USB (para debugging)

# Adicionar biblioteca padrão do Pico
target_link_libraries(Controle_PI_Servo_Temperatura
    pico_stdlib
)

# Incluir diretórios de header files
target_include_directories(Controle_PI_Servo_Temperatura PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Adicionar bibliotecas específicas do projeto (adicione conforme necessário)
target_link_libraries(Controle_PI_Servo_Temperatura
   
)

# Gerar arquivos de saída adicionais (.uf2, .hex, etc.)
pico_add_extra_outputs(Controle_PI_Servo_Temperatura)