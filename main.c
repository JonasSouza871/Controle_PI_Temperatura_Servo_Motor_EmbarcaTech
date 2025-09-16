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
const char *HTML_BODY = "<!DOCTYPE html><html lang=\"pt-BR\"><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /><title>Dashboard de Controle - Pico W</title><script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script><style>:root{--cor-fundo: #f0f2f5;--cor-container: #ffffff;--cor-texto: #333;--cor-primaria: #007bff;--cor-sombra: rgba(0, 0, 0, 0.1);--cor-sucesso: #28a745;--cor-erro: #dc3545;--cor-borda: #dee2e6;}body{font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto,\"Helvetica Neue\", Arial, sans-serif;background-color: var(--cor-fundo);color: var(--cor-texto);margin: 0;padding: 20px;line-height: 1.6;}.container{max-width: 1200px;margin: auto;display: grid;gap: 20px;}header{background: var(--cor-container);padding: 20px;border-radius: 8px;box-shadow: 0 2px 4px var(--cor-sombra);border-left: 5px solid var(--cor-primaria);text-align: center;}h1,h2{margin: 0;color: var(--cor-primaria);}h2{margin-bottom: 15px;border-bottom: 2px solid var(--cor-borda);padding-bottom: 10px;}.card{background-color: var(--cor-container);padding: 20px;border-radius: 8px;box-shadow: 0 2px 4px var(--cor-sombra);}#dashboard{display: grid;grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));gap: 20px;}.status-item{text-align: center;}.status-item h3{margin: 0 0 10px 0;font-size: 1rem;color: #6c757d;}.status-item p{margin: 0;font-size: 1.8rem;font-weight: 500;}#status-container{display: flex;align-items: center;justify-content: center;gap: 10px;}#status-indicator{width: 15px;height: 15px;border-radius: 50%;background-color: #6c757d;transition: background-color 0.5s ease;}#controle form{display: flex;flex-wrap: wrap;gap: 10px;align-items: center;}#controle input[type=\"text\"]{flex-grow: 1;padding: 10px;border: 1px solid var(--cor-borda);border-radius: 5px;font-size: 1rem;}#controle button{padding: 10px 20px;border: none;border-radius: 5px;background-color: var(--cor-primaria);color: white;font-size: 1rem;cursor: pointer;transition: background-color 0.2s ease;}#controle button:hover{background-color: #0056b3;}#feedback-message{margin-top: 10px;font-weight: bold;height: 20px;}.feedback-success{color: var(--cor-sucesso);}.feedback-error{color: var(--cor-erro);}#grafico-container{position: relative;height: 40vh;min-height: 300px;}</style></head><body><div class=\"container\"><header><h1>Painel de Controle de Temperatura</h1></header><main id=\"dashboard\" class=\"card\"><div class=\"status-item\"><h3>Temperatura Atual</h3><p><span id=\"temp-atual\">--</span> °C</p></div><div class=\"status-item\"><h3>Setpoint</h3><p><span id=\"temp-desejada\">--</span> °C</p></div><div class=\"status-item\"><h3>Erro</h3><p><span id=\"erro\">--</span></p></div><div class=\"status-item\"><h3>Ângulo Servo</h3><p><span id=\"angulo-servo\">--</span> °</p></div><div class=\"status-item\"><h3>Motor</h3><p><span id=\"velocidade-motor\">--</span> %</p></div><div class=\"status-item\"><h3>Status</h3><div id=\"status-container\"><span id=\"status-indicator\"></span><p id=\"status-texto\" style=\"font-size: 1.5rem\">Offline</p></div></div></main><section id=\"controle\" class=\"card\"><h2>Controle Remoto</h2><form id=\"setpoint-form\"><input type=\"text\" id=\"novo-setpoint\" placeholder=\"Digite a nova temperatura (ex: 25.5 ou 25,5)\" required /><button type=\"submit\">Aplicar</button></form><p id=\"feedback-message\"></p></section><section id=\"grafico\" class=\"card\"><h2>Histórico de Temperatura (Últimos 15 minutos)</h2><div id=\"grafico-container\"><canvas id=\"tempChart\"></canvas></div></section></div><script>document.addEventListener(\"DOMContentLoaded\", () => {const tempAtualElem = document.getElementById(\"temp-atual\");const tempDesejadaElem = document.getElementById(\"temp-desejada\");const erroElem = document.getElementById(\"erro\");const anguloServoElem = document.getElementById(\"angulo-servo\");const velocidadeMotorElem = document.getElementById(\"velocidade-motor\");const statusIndicator = document.getElementById(\"status-indicator\");const statusTexto = document.getElementById(\"status-texto\");const setpointForm = document.getElementById(\"setpoint-form\");const novoSetpointInput = document.getElementById(\"novo-setpoint\");const feedbackMessage = document.getElementById(\"feedback-message\");const MAX_DATA_POINTS = 900;const ctx = document.getElementById(\"tempChart\").getContext(\"2d\");const tempChart = new Chart(ctx, {type: \"line\",data: {labels: [],datasets: [{label: \"Temperatura Atual (°C)\",data: [],borderColor: \"rgba(220, 53, 69, 1)\",backgroundColor: \"rgba(220, 53, 69, 0.1)\",borderWidth: 2,tension: 0.3,fill: true,},{label: \"Setpoint (°C)\",data: [],borderColor: \"rgba(0, 123, 255, 1)\",borderWidth: 2,borderDash: [5, 5],tension: 0.3,fill: false,},],},options: {responsive: true,maintainAspectRatio: false,scales: {x: {ticks: {maxRotation: 0,autoSkip: true,maxTicksLimit: 10,},},y: {beginAtZero: false,title: {display: true,text: \"Temperatura (°C)\",},},},animation: {duration: 250,},interaction: {intersect: false,mode: \"index\",},},});async function fetchDataAndUpdate() {try {const response = await fetch(\"/status\");if (!response.ok) {throw new Error(`HTTP error! status: ${response.status}`);}const data = await response.json();tempAtualElem.textContent = data.temperatura_atual.toFixed(2);tempDesejadaElem.textContent = data.temperatura_desejada.toFixed(2);erroElem.textContent = data.erro.toFixed(2);anguloServoElem.textContent = data.angulo_alvo.toFixed(1);velocidadeMotorElem.textContent = data.velocidade_ventoinha.toFixed(0);statusIndicator.style.backgroundColor = \"var(--cor-sucesso)\";statusTexto.textContent = \"Operando\";updateChart(data);} catch (error) {console.error(\"Erro ao buscar dados:\", error);statusIndicator.style.backgroundColor = \"var(--cor-erro)\";statusTexto.textContent = \"Erro\";}}function updateChart(data) {const now = new Date().toLocaleTimeString(\"pt-BR\");tempChart.data.labels.push(now);tempChart.data.datasets[0].data.push(data.temperatura_atual);tempChart.data.datasets[1].data.push(data.temperatura_desejada);if (tempChart.data.labels.length > MAX_DATA_POINTS) {tempChart.data.labels.shift();tempChart.data.datasets.forEach((dataset) => {dataset.data.shift();});}tempChart.update();}setpointForm.addEventListener(\"submit\", async (e) => {e.preventDefault();const tempValue = novoSetpointInput.value.trim().replace(\",\", \".\");const newTemp = parseFloat(tempValue);if (isNaN(newTemp)) {showFeedback(\"Por favor, insira um número válido.\", \"error\");return;}try {const response = await fetch(`/set_temperatura?temperatura=${newTemp}`);const result = await response.json();if (result.status === \"success\") {showFeedback(\"Setpoint atualizado com sucesso!\", \"success\");tempDesejadaElem.textContent = result.temperatura_desejada.toFixed(2);novoSetpointInput.value = \"\";} else {throw new Error(result.message || \"Erro desconhecido\");}} catch (error) {console.error(\"Erro ao enviar setpoint:\", error);showFeedback(\"Falha ao comunicar com o dispositivo.\", \"error\");}});function showFeedback(message, type) {feedbackMessage.textContent = message;feedbackMessage.className = type === \"success\" ? \"feedback-success\" : \"feedback-error\";setTimeout(() => {feedbackMessage.textContent = \"\";feedbackMessage.className = \"\";}, 4000);}fetchDataAndUpdate();setInterval(fetchDataAndUpdate, 1000);});</script></body></html>";
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