#include "pico_http_server.h"
#include <string.h>
#include <stdlib.h>
#include "pico/stdio.h"

// --- Variáveis internas da biblioteca ---
#define MAX_HANDLERS 10
static http_request_handler_t handlers[MAX_HANDLERS];
static int handler_count = 0;
static const char *homepage_content = NULL;
static http_content_type_t response_content_type = HTTP_CONTENT_TYPE_HTML;

// Estrutura para gerenciar o estado da conexão
struct http_state
{
    char response[8192]; // Tamanho do buffer de resposta
    size_t len;
    size_t sent;
};

// Callback para enviar dados após a escrita
static err_t http_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

// Roteador de requisições
static void handle_request(struct http_state *hs, const char *req_line)
{
    char *path_end = strchr(req_line, ' ');
    if (path_end == NULL)
    {
        hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }
    size_t path_len = path_end - req_line;
    char path[path_len + 1];
    strncpy(path, req_line, path_len);
    path[path_len] = '\0';

    if ((strcmp(path, "/") == 0) && homepage_content)
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n%s",
                           (int)strlen(homepage_content), homepage_content);

        return;
    }

    // Procura por um handler registrado
    for (int i = 0; i < handler_count; i++)
    {
        if (strstr(path, handlers[i].path) && handlers[i].handler)
        {
            const char *content = handlers[i].handler(req_line);
            const char *content_type_str;
            switch (response_content_type)
            {
            case HTTP_CONTENT_TYPE_JSON:
                content_type_str = "application/json";
                break;
            case HTTP_CONTENT_TYPE_PLAIN:
                content_type_str = "text/plain";
                break;
            case HTTP_CONTENT_TYPE_HTML:
            default:
                content_type_str = "text/html";
                break;
            }
            hs->len = snprintf(hs->response, sizeof(hs->response),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %d\r\n"
                               "Connection: close\r\n\r\n%s",
                               content_type_str, (int)strlen(content), content);
            return;
        }
    }

    // Chegará até aqui se nenhum handler for encontrado
    hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 404 Not Found\r\n\r\n");
}

// Callback principal de recepção de dados
static err_t http_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    struct http_state *hs = (struct http_state *)malloc(sizeof(struct http_state));
    hs->sent = 0;

    char *req = (char *)p->payload;
    if (strstr(req, "GET ") == req)
    {
        req += 4; // Avança o ponteiro após "GET "
        handle_request(hs, req);
    }
    else
    {
        hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent_callback);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

// Callback de nova conexão
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv_callback);
    return ERR_OK;
}

// Função de inicialização
int http_server_init(const char *ssid, const char *password)
{
    if (cyw43_arch_init())
    {
        printf("Falha ao inicializar o Wi-Fi.\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando a %s...\n", ssid);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 15000))
    {
        printf("Falha na conexão Wi-Fi.\n");
        return -1;
    }
    printf("Conectado! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);

    printf("Servidor HTTP iniciado na porta 80.\n");
    return 0;
}

// Implementação das funções de interface
void http_server_set_homepage(const char *html_content)
{
    homepage_content = html_content;
}

void http_server_register_handler(http_request_handler_t handler)
{
    if (handler_count < MAX_HANDLERS)
    {
        handlers[handler_count++] = handler;
    }
}

void http_server_set_content_type(http_content_type_t type)
{
    response_content_type = type;
}

void http_server_parse_float_param(const char *req, const char *param, float *value)
{
    char *found = strstr(req, param);
    if (found)
    {
        *value = atof(found + strlen(param));
    }
}