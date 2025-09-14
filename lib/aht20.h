#ifndef AHT20_H
#define AHT20_H

#include <stdbool.h>
#include "hardware/i2c.h"

/* ---------- Configurações do Sensor AHT20 ---------- */
#define AHT20_I2C_ADDR      0x38

/* ---------- Comandos do AHT20 ---------- */
#define AHT20_CMD_INIT      0xBE
#define AHT20_CMD_TRIGGER   0xAC
#define AHT20_CMD_RESET     0xBA

/* ---------- Estrutura de Dados ---------- */
// Estrutura para armazenar os valores de temperatura e umidade
typedef struct {
    float temperature;
    float humidity;
} AHT20_Data;

/* ---------- API do Sensor AHT20 ---------- */

// Inicializa o sensor AHT20
bool aht20_init(i2c_inst_t *i2c);

// Faz leitura de temperatura e umidade do AHT20
bool aht20_read(i2c_inst_t *i2c, AHT20_Data *data);

// Reseta o sensor AHT20
void aht20_reset(i2c_inst_t *i2c);

// Verifica se o sensor AHT20 está respondendo
bool aht20_check(i2c_inst_t *i2c);

#endif // AHT20_H
