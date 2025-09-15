#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "aht20.h"

// === CONFIGURAÇÕES DE HARDWARE ===
#define PORTA_I2C        i2c0     
#define PINO_SDA         0        
#define PINO_SCL         1       
#define PINO_SERVO       8        // Pino PWM do servo motor

// === CONFIGURAÇÕES DO SERVO ===
#define PULSO_MIN_US     500      // Pulso mínimo do servo (0 graus)
#define PULSO_MAX_US     2500     // Pulso máximo do servo (180 graus)
#define DIVISOR_PWM      125.0f   // Divisor de clock do PWM
#define WRAP_PWM         19999    // Valor wrap para frequência 50Hz

// === CONFIGURAÇÕES DO CONTROLE PI ===
#define GANHO_P          10.0f    // Ganho proporcional (Kp)
#define GANHO_I          0.2f     // Ganho integral (Ki)
#define PERIODO_AMOSTRA  1.0f     // Período de amostragem em segundos
#define INTEGRAL_MIN     -90.0f   // Limite mínimo do termo integral
#define INTEGRAL_MAX     90.0f    // Limite máximo do termo integral

// === VARIÁVEIS GLOBAIS ===
float temperatura_desejada = 28.0;  // Setpoint inicial de temperatura
float termo_integral = 0.0;         // Acumulador do termo integral
uint fatia_pwm;                     // Slice PWM utilizada pelo servo

// === FUNÇÕES AUXILIARES ===
float mapear_valores(float valor, float entrada_min, float entrada_max, float saida_min, float saida_max) {
    // Mapeia valor de uma faixa para outra 
    return (valor - entrada_min) * (saida_max - saida_min) / (entrada_max - entrada_min) + saida_min;
}

// === FUNÇÕES DO SENSOR ===
bool inicializar_sensor(void) {
    i2c_init(PORTA_I2C, 100000);                    
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);     
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);    
    gpio_pull_up(PINO_SDA);                         
    gpio_pull_up(PINO_SCL);                        
    
    if (!aht20_init(PORTA_I2C)) {
        printf("ERRO: Falha ao inicializar sensor AHT20!\n");
        return false;  // Retorna erro se sensor não responder
    }
    printf("Sensor AHT20 inicializado com sucesso.\n");
    return true;
}

bool ler_temperatura(float* temperatura_atual) {
    AHT20_Data dados_sensor;
    if (aht20_read(PORTA_I2C, &dados_sensor)) {
        *temperatura_atual = dados_sensor.temperature;  // Extrai temperatura dos dados
        return true;   // Leitura bem-sucedida
    }
    return false;      // Erro na leitura
}

// === FUNÇÕES DO SERVO ===
void inicializar_servo(void) {
    gpio_set_function(PINO_SERVO, GPIO_FUNC_PWM);    // Configura pino como PWM
    fatia_pwm = pwm_gpio_to_slice_num(PINO_SERVO);   // Obtém slice PWM do pino
    pwm_config configuracao = pwm_get_default_config();
    pwm_config_set_clkdiv(&configuracao, DIVISOR_PWM);  // Define divisor para 50Hz
    pwm_config_set_wrap(&configuracao, WRAP_PWM);        // Define período PWM
    pwm_init(fatia_pwm, &configuracao, true);            // Inicializa e habilita PWM
    printf("Servo motor inicializado no GPIO %d.\n", PINO_SERVO);
}

void definir_angulo_servo(float angulo) {
    // Converte ângulo (0-180°) para largura de pulso em microssegundos
    uint16_t largura_pulso = (uint16_t)mapear_valores(angulo, 0, 180, PULSO_MIN_US, PULSO_MAX_US);
    pwm_set_gpio_level(PINO_SERVO, largura_pulso);   // Aplica o pulso PWM
}

// === FUNÇÕES DE CONTROLE PI ===
float calcular_controle_pi(float temperatura_atual) {
    float erro = temperatura_atual - temperatura_desejada;    // Calcula erro (PV - SP)
    float termo_proporcional = GANHO_P * erro;               // Termo proporcional
    
    termo_integral += GANHO_I * erro * PERIODO_AMOSTRA;      // Acumula termo integral
    
    if (termo_integral > INTEGRAL_MAX) termo_integral = INTEGRAL_MAX; // Anti-windup superior
    if (termo_integral < INTEGRAL_MIN) termo_integral = INTEGRAL_MIN; // Anti-windup inferior
    
    return termo_proporcional + termo_integral;  // Sinal de controle PI
}

void aplicar_controle(float temperatura_atual) {
    float sinal_controle = calcular_controle_pi(temperatura_atual);  // Calcula PI
    float angulo_alvo = 90.0f + sinal_controle;                     // 90° é posição neutra
    
    if (angulo_alvo > 180.0f) angulo_alvo = 180.0f; // Satura em 180°
    if (angulo_alvo < 0.0f) angulo_alvo = 0.0f;     // Satura em 0°
    
    float porcentagem_motor = mapear_valores(angulo_alvo, 0.0f, 180.0f, 0.0f, 100.0f);
    float erro = temperatura_atual - temperatura_desejada;
    
    definir_angulo_servo(angulo_alvo);  // Move servo para posição calculada
    
    // Exibe informações de controle no terminal
    printf("Temp: %.2f C | Setpoint: %.2f C | Erro: %.2f | Angulo: %.1f deg | Motor: %.0f%%\n", 
           temperatura_atual, temperatura_desejada, erro, angulo_alvo, porcentagem_motor);
}

// === FUNÇÕES DE COMUNICAÇÃO SERIAL ===
void verificar_nova_temperatura_serial(void) {
    static char buffer_entrada[20];     // Buffer para armazenar entrada do usuário
    static int contador_chars = 0;      // Contador de caracteres no buffer
    int caractere;

    // Verifica se há caracteres disponíveis na porta serial (não-bloqueante)
    while ((caractere = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        
        if (caractere == '\n' || caractere == '\r') { // Detecta Enter pressionado
            if (contador_chars > 0) {
                buffer_entrada[contador_chars] = '\0';  // Finaliza string

                // Substitui vírgula por ponto para aceitar formato brasileiro
                for (int i = 0; i < contador_chars; i++) {
                    if (buffer_entrada[i] == ',') {
                        buffer_entrada[i] = '.';
                    }
                }
                
                float nova_temperatura = atof(buffer_entrada);  // Converte string para float
                
                // Valida entrada: aceita números positivos ou zero
                if (nova_temperatura > 0.0 || (buffer_entrada[0] == '0')) {
                    temperatura_desejada = nova_temperatura;
                    termo_integral = 0.0;  // Reset integral para evitar overshoots
                    printf("\n>> Setpoint atualizado para %.2f C\n", temperatura_desejada);
                } else {
                    printf("\n>> ERRO: Valor '%s' invalido. Digite um numero.\n", buffer_entrada);
                }
                contador_chars = 0;  // Limpa buffer para próxima entrada
            }
        } else {
            // Adiciona caractere ao buffer se for número, ponto ou vírgula
            if (contador_chars < (sizeof(buffer_entrada) - 1)) {
                if (isdigit(caractere) || caractere == '.' || caractere == ',') {
                    buffer_entrada[contador_chars++] = (char)caractere;
                }
                // Ignora caracteres inválidos silenciosamente
            }
        }
    }
}

// === FUNÇÃO PRINCIPAL ===
int main() {
    stdio_init_all();    // Inicializa sistema de I/O
    sleep_ms(4000);      // Aguarda estabilização do sistema

    printf("\n=== Controle PI de Temperatura com Servo Motor ===\n");

    // Inicializa sensor de temperatura
    if (!inicializar_sensor()) {
        while (1);  // Trava programa se sensor falhar
    }

    // Inicializa servo motor
    inicializar_servo();
    definir_angulo_servo(90);  // Posição inicial: 90° (neutro)

    printf("\nDigite uma nova temperatura (ex: 25.5 ou 25,5) e pressione Enter.\n\n");

    // Loop principal de controle
    while (1) {
        verificar_nova_temperatura_serial();  // Verifica entrada do usuário
        
        float temperatura_atual;
        if (ler_temperatura(&temperatura_atual)) {  // Lê sensor
            aplicar_controle(temperatura_atual);     // Executa controle PI
        } else {
            printf("Erro ao ler dados do sensor AHT20.\n");
        }
        
        sleep_ms(PERIODO_AMOSTRA * 1000);  // Aguarda próximo ciclo de controle
    }
    
    return 0;
}