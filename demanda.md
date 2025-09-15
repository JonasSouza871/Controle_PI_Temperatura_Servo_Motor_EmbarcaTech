# Projeto ForjaTech: Demandas Pendentes (RTOS)

## 📋 Status Geral
- ✅ **Pessoa 1**: Lógica de controle PI implementada (base pronta)
- ⏳ **Pessoa 2**: Lógica de Feedbacks (pendente)
- ⏳ **Pessoa 3**: Interface Web (pendente)
- ⏳ **Pessoa 4**: FreeRTOS (pendente)

---

## 🌐 Pessoa 3: Interface Web

### Funcionalidades Obrigatórias

#### 📊 **Dashboard Principal**
- [ ] **Monitoramento em Tempo Real**
  - Temperatura atual (°C)
  - Setpoint configurado (°C)
  - Erro de temperatura
  - Ângulo do servo (graus)
  - Porcentagem do motor (0-100%)
  - Status do sistema (Operando/Parado/Erro)

#### ⚙️ **Controle Remoto**
- [ ] **Input de Setpoint**
  - Campo numérico para nova temperatura desejada
  - Validação de entrada (aceitar vírgula e ponto)
  - Botão "Aplicar" para enviar novo valor
  - Feedback visual de confirmação

#### 📈 **Visualização de Dados**
- [ ] **Gráfico de Temperatura**
  - Gráfico em tempo real (últimos 10-15 minutos)
  - Linha da temperatura atual
  - Linha do setpoint
  - Atualização automática a cada segundo


### Especificações Técnicas

#### 🎨 **Frontend (HTML/CSS/JavaScript)**
- [ ] Interface responsiva (desktop e mobile)
- [ ] Uso de WebSockets para comunicação em tempo real
- [ ] Biblioteca de gráficos (Chart.js recomendado)
- [ ] Design moderno e intuitivo

#### 🔌 **Backend (Servidor no Pico W)**
- [ ] Servidor HTTP para servir a página
- [ ] WebSocket server para dados em tempo real
- [ ] API REST para comandos (POST/GET)
- [ ] Configuração Wi-Fi e LwIP

#### 📡 **Comunicação**
- [ ] **Estrutura JSON para dados:**
```json
{
  "temperatura_atual": 25.8,
  "setpoint": 28.0,
  "erro": -2.2,
  "angulo_servo": 115.5,
  "porcentagem_motor": 64,
  "status_sistema": "operando"
}
```

---

## 💡 Pessoa 2: Lógica de Feedbacks

### Hardware a Implementar

#### 🖥️ **Display OLED**
- [ ] **Tela Principal**
  - Temperatura atual (grande, destaque)
  - Setpoint configurado
  - Status do sistema
  - Tempo decorrido de operação

- [ ] **Telas Secundárias**
  - Gráfico de barras da temperatura
  - Informações detalhadas (erro, PID)
  - Menu de configurações
  - Navegação por botões

#### 💡 **LEDs RGB**
- [ ] **Indicação por Cores**
  - 🟢 **Verde**: Sistema operando normalmente
  - 🔵 **Azul**: Sistema em standby
  - 🟡 **Amarelo**: Aquecendo (temperatura < setpoint)
  - 🔴 **Vermelho**: Erro ou temperatura crítica
  - 🟣 **Roxo**: Modo de configuração

- [ ] **Padrões de Pisca**
  - Pisca lento: operação normal
  - Pisca rápido: alerta/erro
  - Pisca alternado: conectando Wi-Fi

#### 🔊 **Buzzer**
- [ ] **Alertas Sonoros**
  - 1 bip curto: confirmação de comando
  - 2 bips: erro de entrada
  - 3 bips longos: temperatura crítica atingida
  - Bip contínuo: erro de sensor
  - Melodia: processo finalizado com sucesso

#### 🔘 **Botões**
-  Passagem entre telas

- [ ] **Implementações Necessárias**
  - Debounce de botões (20-50ms)
  - Detecção de pressionamento longo
  - Navegação em menus do display
  - Configuração de setpoint local

### Lógicas de Controle

#### 📟 **Sistema de Menus**
- [ ] Menu principal → Submenu configurações
- [ ] Navegação circular entre opções
- [ ] Confirmação visual de seleções

#### 🚨 **Sistema de Alertas**
- [ ] Verificação de limites de temperatura
- [ ] Detecção de erro de sensor
- [ ] Timeout de comunicação
- [ ] Alertas visuais + sonoros coordenados

---

## ⚙️ Pessoa 4: FreeRTOS

### Arquitetura do Sistema

#### 📋 **Tasks Principais**
- [ ] **ControlTask** (Prioridade: ALTA)
  - Leitura do sensor de temperatura
  - Cálculo do controle PI
  - Controle do servo motor
  - Período: 1 segundo

- [ ] **LocalUITask** (Prioridade: MÉDIA)
  - Atualização do display OLED
  - Leitura de botões com debounce
  - Controle de LEDs e buzzer
  - Período: 100ms

- [ ] **WebServiceTask** (Prioridade: MÉDIA)
  - Servidor web HTTP
  - WebSocket para dados em tempo real
  - Processamento de comandos remotos
  - Período: variável (orientada por eventos)

- [ ] **MonitorTask** (Prioridade: BAIXA)
  - Watchdog do sistema
  - Logging de eventos
  - Estatísticas de desempenho
  - Período: 5 segundos

### Comunicação Entre Tasks

#### 📨 **Filas Recomendadas**
- [ ] **`xFilaComandos`** (Queue)
  - **Tamanho**: 10 itens
  - **Tipo**: Estrutura de comandos
  - **Remetentes**: LocalUITask, WebServiceTask
  - **Destinatário**: ControlTask
  ```c
  typedef struct {
      enum { CMD_SET_TEMP, CMD_START, CMD_STOP, CMD_RESET } tipo;
      float valor;
      uint8_t origem; // 0=local, 1=web
  } Comando_t;
  ```

- [ ] **`xFilaDados`** (Queue)
  - **Tamanho**: 5 itens
  - **Tipo**: Estrutura de dados do sistema
  - **Remetente**: ControlTask
  - **Destinatários**: LocalUITask, WebServiceTask
  ```c
  typedef struct {
      float temperatura_atual;
      float setpoint;
      float erro;
      float angulo_servo;
      uint8_t status_sistema;
      uint32_t timestamp;
  } DadosSistema_t;
  ```

- [ ] **`xFilaAlertas`** (Queue)
  - **Tamanho**: 15 itens
  - **Tipo**: Estrutura de alertas
  - **Remetente**: Qualquer task
  - **Destinatário**: LocalUITask
  ```c
  typedef struct {
      enum { ALERT_INFO, ALERT_WARNING, ALERT_ERROR } nivel;
      uint16_t codigo;
      char mensagem[32];
  } Alerta_t;
  ```

#### 🔐 **Mutexes Necessários**
- [ ] **`xMutexI2C`**
  - Proteção do barramento I2C (sensor + display)
  - Usado por: ControlTask, LocalUITask

- [ ] **`xMutexSerial`**
  - Proteção da saída serial/debug
  - Usado por: Todas as tasks

#### 🚩 **Event Groups**
- [ ] **`xEventGroupSistema`**
  - **Bits de Status**:
    - `BIT_SISTEMA_RODANDO` (0x01)
    - `BIT_SENSOR_OK` (0x02) 
    - `BIT_WIFI_CONECTADO` (0x04)
    - `BIT_WEB_CLIENT_CONECTADO` (0x08)
    - `BIT_ERRO_CRITICO` (0x10)

#### 📊 **Monitoramento**
- [ ] Implementar estatísticas de CPU por task
- [ ] Monitoramento de stack overflow
- [ ] Watchdog para tasks críticas
- [ ] Sistema de logging com prioridades
