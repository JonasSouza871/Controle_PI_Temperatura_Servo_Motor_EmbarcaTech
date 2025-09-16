#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "aht20.h"
#include "pico_http_server.h"

// === CONFIGURAÇÕES DE HARDWARE ===
#define PORTA_I2C i2c0
#define PINO_SDA 0
#define PINO_SCL 1
#define PINO_SERVO 8

// === CONFIGURAÇÕES DA VENTOINHA (L298N) ===
#define PINO_IN1 18
#define PINO_IN2 19
#define PINO_ENA_PWM 20

// === CONFIGURAÇÕES DO SERVO ===
#define PULSO_MIN_US 500   // Pulso para 0 graus
#define PULSO_MAX_US 2500  // Pulso para 180 graus
#define DIVISOR_PWM 125.0f // Divisor de clock para frequência de ~50Hz
#define WRAP_PWM 19999     // Valor do período PWM

// === CONFIGURAÇÕES PWM DA VENTOINHA ===
#define DIVISOR_PWM_VENTOINHA 4.0f
#define WRAP_PWM_VENTOINHA 999 // Resolução do PWM da ventoinha (0-999)

// === CONFIGURAÇÕES DO CONTROLE PI ===
#define GANHO_P 10.0f        // Ganho Proporcional (Kp)
#define GANHO_I 0.2f         // Ganho Integral (Ki)
#define PERIODO_AMOSTRA 1.0f // Intervalo em segundos entre cada ciclo de controle
#define INTEGRAL_MIN -90.0f  // Limite inferior do termo integral (anti-windup)
#define INTEGRAL_MAX 90.0f   // Limite superior do termo integral (anti-windup)

// === VARIÁVEIS GLOBAIS ===
float temperatura_desejada = 28.0;
float termo_integral = 0.0;
uint fatia_pwm_servo;
uint fatia_pwm_ventoinha;
float http_temperatura_atual;
float http_erro;
float http_angulo_alvo;
float http_velocidade_ventoinha;

// === PÁGINA HTTP ===
const char *HTML_BODY = NULL;
const char *PATH_TO_INDEX = "DEFINA AQUI O PATH PARA O INDEX.HTML";
const char *SSID = "DEFINA AQUI O SSID DA REDE";
const char *SENHA = "DEFINA AQUI A SENHA DA REDE";

// Função para tratar a requisição "/status"
const char *status_handler(const char *request)
{
    static char response_buffer[256];
    http_server_set_content_type(HTTP_CONTENT_TYPE_JSON);
    snprintf(response_buffer, sizeof(response_buffer),
             "{\"temperatura_atual\": %.2f, \"temperatura_desejada\": %.2f, \"erro\": %.2f, \"angulo_alvo\": %.2f, \"velocidade_ventoinha\": %.2f}",
             http_temperatura_atual,
             temperatura_desejada,
             http_erro,
             http_angulo_alvo,
             http_velocidade_ventoinha);
    return response_buffer;
}

// Função para tratar a requisição "/set_temperatura?"
const char *set_temperatura_handler(const char *request)
{
    http_server_set_content_type(HTTP_CONTENT_TYPE_JSON);

    float new_temperatura_desejada;

    http_server_parse_float_param(request, "temperatura=", &new_temperatura_desejada);

    temperatura_desejada = new_temperatura_desejada;

    static char response_buffer[128];

    snprintf(response_buffer, sizeof(response_buffer),
             "{\"status\":\"success\", \"message\":\"Settings updated\", \"temperatura_desejada\":%.2f}",
             temperatura_desejada);

    return response_buffer;
}

// Mapeia um valor de uma faixa de entrada para uma faixa de saída.
float mapear_valores(float valor, float entrada_min, float entrada_max, float saida_min, float saida_max)
{
    return (valor - entrada_min) * (saida_max - saida_min) / (entrada_max - entrada_min) + saida_min;
}

// Configura a comunicação I2C e inicializa o sensor AHT20.
bool inicializar_sensor(void)
{
    i2c_init(PORTA_I2C, 100000);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SDA);
    gpio_pull_up(PINO_SCL);

    if (!aht20_init(PORTA_I2C))
    {
        printf("ERRO: Falha ao inicializar sensor AHT20!\n");
        return false;
    }
    printf("Sensor AHT20 inicializado com sucesso.\n");
    return true;
}

// Lê a temperatura atual do sensor AHT20.
bool ler_temperatura(float *temperatura_atual)
{
    AHT20_Data dados_sensor;
    if (aht20_read(PORTA_I2C, &dados_sensor))
    {
        *temperatura_atual = dados_sensor.temperature;
        return true;
    }
    return false;
}

// Configura o pino do servo para operar com PWM.
void inicializar_servo(void)
{
    gpio_set_function(PINO_SERVO, GPIO_FUNC_PWM);
    fatia_pwm_servo = pwm_gpio_to_slice_num(PINO_SERVO);
    pwm_config configuracao = pwm_get_default_config();
    pwm_config_set_clkdiv(&configuracao, DIVISOR_PWM);
    pwm_config_set_wrap(&configuracao, WRAP_PWM);
    pwm_init(fatia_pwm_servo, &configuracao, true);
    printf("Servo motor inicializado no GPIO %d.\n", PINO_SERVO);
}

// Converte o ângulo (0-180) para a largura de pulso e o aplica no pino do servo.
void definir_angulo_servo(float angulo)
{
    uint16_t largura_pulso = (uint16_t)mapear_valores(angulo, 0, 180, PULSO_MIN_US, PULSO_MAX_US);
    pwm_set_gpio_level(PINO_SERVO, largura_pulso);
}

// Configura os pinos de controle e o PWM da ventoinha.
void inicializar_ventoinha(void)
{
    gpio_init(PINO_IN1);
    gpio_init(PINO_IN2);
    gpio_set_dir(PINO_IN1, GPIO_OUT);
    gpio_set_dir(PINO_IN2, GPIO_OUT);

    gpio_put(PINO_IN1, true); // Define a direção de rotação
    gpio_put(PINO_IN2, false);

    gpio_set_function(PINO_ENA_PWM, GPIO_FUNC_PWM);
    fatia_pwm_ventoinha = pwm_gpio_to_slice_num(PINO_ENA_PWM);

    pwm_config config_ventoinha = pwm_get_default_config();
    pwm_config_set_clkdiv(&config_ventoinha, DIVISOR_PWM_VENTOINHA);
    pwm_config_set_wrap(&config_ventoinha, WRAP_PWM_VENTOINHA);
    pwm_init(fatia_pwm_ventoinha, &config_ventoinha, true);

    pwm_set_gpio_level(PINO_ENA_PWM, 0); // Garante que a ventoinha comece desligada
    printf("Ventoinha inicializada nos GPIOs %d, %d, %d.\n", PINO_IN1, PINO_IN2, PINO_ENA_PWM);
}

// Define a velocidade da ventoinha (0 a 100%).
void definir_velocidade_ventoinha(float porcentagem)
{
    if (porcentagem > 100.0f)
        porcentagem = 100.0f;
    if (porcentagem < 0.0f)
        porcentagem = 0.0f;

    uint16_t valor_pwm = (uint16_t)(porcentagem * WRAP_PWM_VENTOINHA / 100.0f);
    pwm_set_gpio_level(PINO_ENA_PWM, valor_pwm);
}

// Calcula o sinal de controle PI com base na temperatura atual.
float calcular_controle_pi(float temperatura_atual)
{
    float erro = temperatura_atual - temperatura_desejada;
    float termo_proporcional = GANHO_P * erro;

    termo_integral += GANHO_I * erro * PERIODO_AMOSTRA; // Acumula o erro

    // Limita o termo integral para evitar sobrecarga (anti-windup)
    if (termo_integral > INTEGRAL_MAX)
        termo_integral = INTEGRAL_MAX;
    if (termo_integral < INTEGRAL_MIN)
        termo_integral = INTEGRAL_MIN;

    return termo_proporcional + termo_integral;
}

// Aplica o controle PI ao servo e à ventoinha.
void aplicar_controle(float temperatura_atual)
{
    float erro = temperatura_atual - temperatura_desejada;
    float sinal_controle = calcular_controle_pi(temperatura_atual);

    // O ângulo alvo parte de 90° e é ajustado pelo sinal de controle
    float angulo_alvo = 90.0f + sinal_controle;

    // Garante que o ângulo do servo permaneça dentro dos limites físicos
    if (angulo_alvo > 180.0f)
        angulo_alvo = 180.0f;
    if (angulo_alvo < 0.0f)
        angulo_alvo = 0.0f;

    // Mapeia o ângulo do servo (0-180°) para a velocidade da ventoinha (0-100%)
    float velocidade_ventoinha = mapear_valores(angulo_alvo, 0.0f, 180.0f, 0.0f, 100.0f);

    definir_angulo_servo(angulo_alvo);
    definir_velocidade_ventoinha(velocidade_ventoinha);

    // Imprime o status atual do sistema no monitor serial
    printf("Temp: %.2f C | Setpoint: %.2f C | Erro: %.2f | Servo: %.1f deg | Ventoinha (motor): %.0f%%\n",
           temperatura_atual, temperatura_desejada, erro, angulo_alvo, velocidade_ventoinha);

    // Atualiza variáveis globais http
    http_temperatura_atual = temperatura_atual;
    http_erro = erro;
    http_angulo_alvo = angulo_alvo;
    http_velocidade_ventoinha = velocidade_ventoinha;
}

// Verifica se o usuário digitou uma nova temperatura via serial.
void verificar_nova_temperatura_serial(void)
{
    static char buffer_entrada[20];
    static int contador_chars = 0;
    int caractere;

    while ((caractere = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
    {

        if (caractere == '\n' || caractere == '\r')
        { // Se o usuário apertou Enter
            if (contador_chars > 0)
            {
                buffer_entrada[contador_chars] = '\0';

                for (int i = 0; i < contador_chars; i++)
                {
                    if (buffer_entrada[i] == ',')
                        buffer_entrada[i] = '.';
                }

                float nova_temperatura = atof(buffer_entrada);

                // Valida a entrada para aceitar apenas números válidos
                if (nova_temperatura > 0.0 || (strcmp(buffer_entrada, "0") == 0) || (strcmp(buffer_entrada, "0.0") == 0))
                {
                    temperatura_desejada = nova_temperatura;
                    termo_integral = 0.0; // Reseta o termo integral
                    printf("\n>> Setpoint atualizado para %.2f C\n", temperatura_desejada);
                }
                else
                {
                    printf("\n>> ERRO: Valor '%s' invalido. Digite um numero.\n", buffer_entrada);
                }
                contador_chars = 0; // Limpa o buffer
            }
        }
        else
        {
            if (contador_chars < (sizeof(buffer_entrada) - 1))
            {
                if (isdigit(caractere) || caractere == '.' || caractere == ',')
                {
                    buffer_entrada[contador_chars++] = (char)caractere;
                }
            }
        }
    }
}

// Função Principal
int main()
{
    stdio_init_all();
    sleep_ms(4000); // Aguarda a conexão serial ser estabelecida

    // Inicia o servidor com sua rede e senha
    if (http_server_init(SSID, SENHA))
    {
        printf("Falha ao iniciar o servidor.\n");
        while (1)
            ;
    }

    // Carrega a página http
    HTML_BODY = http_server_read_html_file(PATH_TO_INDEX);

    // Definição da página http
    http_server_set_homepage(HTML_BODY);

    // Cadastra o handler para a rota "/status"
    http_server_register_handler((http_request_handler_t){"/status", &status_handler});

    // Cadastra o handler para a rota "/set_temperatura?"
    http_server_register_handler((http_request_handler_t){"/set_temperatura?", &set_temperatura_handler});

    printf("\n=== Controle PI de Temperatura com Servo Motor e Ventoinha ===\n");

    if (!inicializar_sensor())
    {
        while (1)
            ; // Trava se o sensor falhar
    }

    inicializar_servo();
    definir_angulo_servo(90); // Posição inicial do servo
    inicializar_ventoinha();

    printf("\nDigite uma nova temperatura (ex: 25.5 ou 25,5) e pressione Enter.\n\n");

    // Loop principal do sistema
    while (1)
    {
        cyw43_arch_poll();
        verificar_nova_temperatura_serial();

        float temperatura_atual;
        if (ler_temperatura(&temperatura_atual))
        {
            aplicar_controle(temperatura_atual);
        }
        else
        {
            printf("Erro ao ler dados do sensor AHT20.\n");
        }

        sleep_ms(PERIODO_AMOSTRA * 1000);
    }

    return 0;
}