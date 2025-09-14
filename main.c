#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"

// --- Configurações da Porta I2C ---
#define I2C_PORT        i2c0
#define I2C_SDA_PIN     0
#define I2C_SCL_PIN     1

int main() {
    // 1. Inicializa a comunicação com o computador (USB)
    stdio_init_all();
    sleep_ms(2000); // Um tempo para a porta serial conectar

    printf("--- Leitor AHT20 Iniciado ---\n");

    // 2. Inicializa a porta I2C0 nos pinos GPIO 0 e 1
    i2c_init(I2C_PORT, 100000); // 100kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // 3. Tenta inicializar o sensor AHT20
    // Se não conseguir, imprime um erro e trava o programa.
    if (!aht20_init(I2C_PORT)) {
        printf("ERRO: Falha ao inicializar o sensor AHT20!\n");
        printf("Verifique as conexoes fisicas.\n");
        while (1); // Trava aqui se houver erro
    }
    
    printf("Sensor AHT20 inicializado com sucesso. Iniciando leituras...\n\n");

    // Estrutura para receber os dados do sensor
    AHT20_Data dados;

    // 4. Loop infinito para fazer as leituras
    while (1) {
        if (aht20_read(I2C_PORT, &dados)) {
            // Se a leitura for bem-sucedida, imprime os valores
            printf("Temperatura: %.2f C  |  Umidade: %.2f %%\n", dados.temperature, dados.humidity);
        } else {
            // Se a leitura falhar
            printf("Erro ao ler dados do sensor.\n");
        }
        
        // Espera 2 segundos para a próxima leitura
        sleep_ms(2000);
    }

    return 0;
}