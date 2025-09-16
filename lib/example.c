#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico_http_server.h"

// Conteúdo da página HTML
const char HTML_BODY[] = "<!DOCTYPE html><html><body><h1>Ola, Pico W!</h1></body></html>";

float g_temp_min = 20.0;
float g_temp_max = 30.0;

// Variáveis globais de dados
float current_temperature = 25.5f;

// Função para tratar a requisição "/data"
const char *data_handler(const char *request)
{
    static char response_buffer[128];
    // Define o tipo de resposta como JSON
    http_server_set_content_type(HTTP_CONTENT_TYPE_JSON);
    snprintf(response_buffer, sizeof(response_buffer), "{\"temperatura\": %.2f}", current_temperature);
    return response_buffer;
}

const char *settings_handler(const char *request)
{
    // Altera o tipo de conteúdo para JSON
    http_server_set_content_type(HTTP_CONTENT_TYPE_JSON);

    // Variáveis locais para armazenar os valores temporariamente
    float new_temp_min, new_temp_max;

    // Use a função que você adicionou na biblioteca para extrair os valores.
    // O nome do parâmetro deve ser exato, incluindo o '='.
    http_server_parse_float_param(request, "temp_min=", &new_temp_min);
    http_server_parse_float_param(request, "temp_max=", &new_temp_max);

    // Atualiza as variáveis globais
    g_temp_min = new_temp_min;
    g_temp_max = new_temp_max;

    // Constrói a resposta JSON para o cliente
    static char response[128]; // Use um buffer estático ou alocado
    snprintf(response, sizeof(response),
             "{\"status\":\"success\", \"message\":\"Settings updated\", \"temp_min\":%.2f, \"temp_max\":%.2f}",
             g_temp_min, g_temp_max);

    return response;
}

int main()
{
    stdio_init_all();
    sleep_ms(1000);

    // Inicia o servidor com sua rede e senha
    if (http_server_init("Familia Luz", "65327890"))
    {
        printf("Falha ao iniciar o servidor.\n");
        while (1)
            ;
    }

    // Define a página inicial
    http_server_set_homepage(HTML_BODY);

    // Cadastra o manipulador para a URL /data
    http_request_handler_t data_req = {
        .path = "/data",
        .handler = &data_handler};
    http_server_register_handler(data_req);

    /*
       Outra forma de cadastrar manipuladores de rotas.
       Observe que nesse caso, essa rota tem atributos.
       Logo, precisamos colocar uma interrogação para
       identificar
    */
    http_server_register_handler((http_request_handler_t){"/set_settings?", &settings_handler});

    while (1)
    {
        // Seu código principal (leitura de sensores, etc.)
        cyw43_arch_poll();
        printf("Temperatura máxima: %f\nTemperatura mínima: %f\n", g_temp_max, g_temp_min);
        sleep_ms(1000);
    }
}