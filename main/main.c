// Chamadas de bibliotecas externas

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#include "time.h"
#include "sys/time.h"

#include <stdlib.h>
#include "driver/gpio.h"

// Coisas do bluetooth

#define SPP_TAG "LOGS"            // Tag para logs
#define SPP_SERVER_NAME "YURIESP" // Nome do servidor bluetooth
#define SPP_SHOW_DATA 0
#define SPP_SHOW_MODE SPP_SHOW_DATA // Config do modo de exibição

// static const char local_device_name[] = SPP_SERVER_NAME;
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;

// static struct timeval time_new, time_old;
// static long data_num = 0;

// Parâmetros de segurança e configuração
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE; // Requer autenticação
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;    // Configuração do papel do dispositivo

// Callback do SPP
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

// Variáveis para comunicação Bluetooth
uint32_t bt_handle;
uint8_t bt_buffer[16];
bool bt_data_flag = 0;

// Função para obter dados do Bluetooth
int get_string_bt(char *msg)
{
    if (bt_data_flag)
    {
        strcpy(msg, (char *)bt_buffer);
        bt_data_flag = 0;
        return 1;
    }
    return 0;
}

// Função para enviar dados pelo Bluetooth
void send_string_bt(char *msg)
{
    int len = strlen(msg);
    esp_spp_write(bt_handle, len, (uint8_t *)msg);
}

// Coisas do GPIO

// Definição de pinos GPIO para botões
#define BUTTON_ONE GPIO_NUM_4    // Botão 1 entrada 4
#define BUTTON_TWO GPIO_NUM_5    // Botão 2 entrada 5
#define BUTTON_THREE GPIO_NUM_18 // Botão 3 entrada 18
#define BUTTON_FOUR GPIO_NUM_19  // Botão 4 entrada 19

#define SUCCESS_BUTTON GPIO_NUM_2  // Botão para sucesso
#define FAILURE_BUTTON GPIO_NUM_15 // Botão para falha

// Definições para senha
#define TAMANHO_SENHA 6 // Tamanho do vetor senha
#define NUM_BUTTONS 4   // Quantidade de botoes

// Configuração dos pinos de entrada e saída
#define GPIO_INPUT_PIN_SEL ((1ULL << BUTTON_ONE) | (1ULL << BUTTON_TWO) | (1ULL << BUTTON_THREE) | (1ULL << BUTTON_FOUR))
#define GPIO_OUTPUT_PIN_SEL ((1ULL << SUCCESS_BUTTON) | (1ULL << FAILURE_BUTTON))

// Vetor senha predefinida (colocar em memória não volátil)
int senha[TAMANHO_SENHA] = {4, 3, 4, 1, 2, 3};

// Associação de valores aos botões
int valores_botoes[NUM_BUTTONS] = {1, 2, 3, 4};
// Vetor onde irá entrar a senha do usuário
int input_senha[TAMANHO_SENHA] = {0};
// Variável para dizer em qual lugar do vetor senha estamos
int input_index = 0;

// Coisas RTC e VNS
#define STORAGE_NAMESPACE "storage"
#define LOG_CAPACITY 10

static uint8_t current_marker = 0x55;
static uint32_t current_log_index = 0;

// Função para salvar um valor na NVS
void save_to_nvs(const char *key, const char *value)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "Erro ao abrir NVS (%s)!", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs_handle, key, value);
    if (err == ESP_OK)
    {
        nvs_commit(nvs_handle);
        ESP_LOGI(SPP_TAG, "Salvo com sucesso: %s = %s", key, value);
    }
    else
    {
        ESP_LOGE(SPP_TAG, "Erro ao salvar (%s)!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

// Função para salvar um log na NVS
void save_log_to_nvs(const char *log_key, const char *log_entry)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "Erro ao abrir NVS para logs (%s)!", esp_err_to_name(err));
        return;
    }

    char key[16];
    snprintf(key, sizeof(key), "%s_%lu", log_key, current_log_index);

    err = nvs_set_str(nvs_handle, key, log_entry);
    if (err == ESP_OK)
    {
        nvs_commit(nvs_handle);
        ESP_LOGI(SPP_TAG, "Log salvo: %s = %s", key, log_entry);

        current_log_index = (current_log_index + 1) % LOG_CAPACITY;
        current_marker = (current_marker == 0x55) ? 0xAA : 0x55;
    }
    else
    {
        ESP_LOGE(SPP_TAG, "Erro ao salvar log (%s)!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

// Função para ler logs da NVS
void read_logs_from_nvs(const char *log_key)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "Erro ao abrir NVS para leitura de logs (%s)!", esp_err_to_name(err));
        return;
    }

    char key[16];
    char log_entry[128];
    size_t required_size;

    ESP_LOGI(SPP_TAG, "Últimos %d logs:", LOG_CAPACITY);
    for (uint32_t i = 0; i < LOG_CAPACITY; i++)
    {
        snprintf(key, sizeof(key), "%s_%lu", log_key, i);

        required_size = sizeof(log_entry);
        err = nvs_get_str(nvs_handle, key, log_entry, &required_size);
        if (err == ESP_OK)
        {
            ESP_LOGI(SPP_TAG, "Log %lu: %s", i, log_entry);
        }
        else
        {
            ESP_LOGW(SPP_TAG, "Log %lu não encontrado.", i);
        }
    }

    nvs_close(nvs_handle);
}

// Comando para atualizar a data
void cmd_data(char *nova_data)
{
    struct tm tm;
    if (strptime(nova_data, "%d-%m-%Y", &tm) == NULL)
    {
        ESP_LOGE(SPP_TAG, "Formato de data inválido. Use DD-MM-YY.");
        return;
    }

    time_t t = time(NULL);
    struct tm *current_time = localtime(&t);

    current_time->tm_year = tm.tm_year;
    current_time->tm_mon = tm.tm_mon;
    current_time->tm_mday = tm.tm_mday;

    const struct timeval tv = {.tv_sec = mktime(current_time), .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(SPP_TAG, "Data atualizada para: %s", nova_data);

    save_to_nvs("data", nova_data);
}

// Comando para atualizar a hora
void cmd_hora(char *nova_hora)
{
    struct tm tm;
    if (strptime(nova_hora, "%H:%M:%S", &tm) == NULL)
    {
        ESP_LOGE(SPP_TAG, "Formato de hora inválido. Use HH:MM:SS.");
        return;
    }

    time_t t = time(NULL);
    struct tm *current_time = localtime(&t);

    current_time->tm_hour = tm.tm_hour;
    current_time->tm_min = tm.tm_min;
    current_time->tm_sec = tm.tm_sec;

    const struct timeval tv = {.tv_sec = mktime(current_time), .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(SPP_TAG, "Hora atualizada para: %s", nova_hora);

    save_to_nvs("hora", nova_hora);
}

// Comando para mostrar a data e a hora
void cmd_relogio()
{
    time_t now;
    time(&now);
    struct tm *timeinfo = localtime(&now);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", timeinfo);
    ESP_LOGI(SPP_TAG, "Data e Hora atuais: %s\n", buffer);
}

// Comando para registrar um log
void cmd_log()
{
    time_t now;
    time(&now);
    struct tm *timeinfo = localtime(&now);

    uint8_t dia = timeinfo->tm_mday;
    uint8_t mes = timeinfo->tm_mon + 1;
    uint8_t ano = timeinfo->tm_year - 100;
    uint8_t hora = timeinfo->tm_hour;
    uint8_t minuto = timeinfo->tm_min;
    uint8_t segundo = timeinfo->tm_sec;

    char log_entry[64];
    snprintf(log_entry, sizeof(log_entry), "%02X %02u %02u %02u %02u %02u %02u 00", current_marker, dia, mes, ano, hora, minuto, segundo);

    save_log_to_nvs("log", log_entry);
}

// Coisas da state machine

typedef enum
{
    STATE_RUNNING,
    STATE_CHANGE_PASSWORD,
    STATE_CHANGE_HORA,
    STATE_CHANGE_DATA
} state_t;

state_t current_state = STATE_RUNNING;

void state_running(char received_msg[128])
{
    if (strcmp(received_msg, "senha") == 0)
    {
        ESP_LOGI(SPP_TAG, "Digite os 6 dígitos da nova senha:\n");
        send_string_bt("Digite os 6 dígitos da nova senha:\n");
        current_state = STATE_CHANGE_PASSWORD;
    }
    else if (strcmp(received_msg, "abre") == 0)
    {
        gpio_set_level(SUCCESS_BUTTON, 1); // Aciona o GPIO de sucesso
        vTaskDelay(pdMS_TO_TICKS(2000));   // Aguarda 2 segundo
        gpio_set_level(SUCCESS_BUTTON, 0); // Desliga o GPIO
        int dentro = 1;
        while (dentro)
        {
            // Testando todos os botões
            for (int i = 0; i < NUM_BUTTONS; i++)
            {

                // Variável que ativa quando qualquer botão é pressionado
                int button_gpio = (i == 0) ? BUTTON_ONE : (i == 1) ? BUTTON_TWO
                                                      : (i == 2)   ? BUTTON_THREE
                                                                   : BUTTON_FOUR;

                // Se um botão é pressionado
                if (gpio_get_level(button_gpio) == 1)
                {

                    // Exibo o botão pressionado
                    // printf("%d\n", valores_botoes[i]);
                    // Se o index é menor que o tamanho da senha
                    if (input_index < TAMANHO_SENHA)
                    {
                        // Atribuo o valor i ao próximo index
                        input_senha[input_index++] = valores_botoes[i];
                    }
                    // Se o botão ainda está pressionado
                    while (gpio_get_level(button_gpio) == 1)
                    {
                        // Espero um pouco
                        vTaskDelay(pdMS_TO_TICKS(50));
                    }
                }
            }
            // Se preenchemos todo o vetor senha do usuário
            if (input_index == TAMANHO_SENHA)
            {

                // Assumimos que ele acertou
                int sucesso = 1;
                // Vamos verificar se ele acertou comparado cada item do vetor com a senha
                for (int i = 0; i < TAMANHO_SENHA; i++)
                {
                    // Se algum desses itens não forem iguais ao da senha
                    if (input_senha[i] != senha[i])
                    {
                        // Ele falhou
                        sucesso = 0;
                        break;
                    }
                }
                if (sucesso)
                {
                    // Se ele acertou
                    send_string_bt("Senha correta!\n");
                    ESP_LOGI(SPP_TAG, "Senha correta! \n");
                    cmd_log();
                    gpio_set_level(SUCCESS_BUTTON, 1); // Aciona o GPIO de sucesso
                    vTaskDelay(pdMS_TO_TICKS(1000));   // Aguarda 1 segundo
                    gpio_set_level(SUCCESS_BUTTON, 0); // Desliga o GPIO
                    dentro = 0;
                }
                else
                {
                    // Se ele errou
                    send_string_bt("Senha incorreta. \n");
                    ESP_LOGI(SPP_TAG, "Senha incorreta! \n");
                    gpio_set_level(FAILURE_BUTTON, 1); // Aciona o GPIO de falha
                    vTaskDelay(pdMS_TO_TICKS(1000));   // Aguarda 1 segundo
                    gpio_set_level(FAILURE_BUTTON, 0); // Desliga o GPIO
                    dentro = 0;
                }

                // Resetamos o vetor senha para tentar de novo
                for (int i = 0; i < TAMANHO_SENHA; i++)
                {
                    input_senha[i] = 0;
                }
                input_index = 0;
            }

            // Esperamos um pouquinho
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    else if (strcmp(received_msg, "data") == 0)
    {
        ESP_LOGI(SPP_TAG, "Digite DDMMAA:\n");
        send_string_bt("Digite DDMMAA:\n");
        current_state = STATE_CHANGE_DATA;
    }
    else if (strcmp(received_msg, "hora") == 0)
    {
        ESP_LOGI(SPP_TAG, "Digite hhmmss:\n");
        send_string_bt("Digite hhmmss:\n");
        current_state = STATE_CHANGE_HORA;
    }
    else if (strcmp(received_msg, "relogio") == 0)
    {
        cmd_relogio();
    }
    else if (strcmp(received_msg, "log") == 0)
    {
        read_logs_from_nvs("log");
    }
    else
    {
        ESP_LOGW(SPP_TAG, "Unknown command: %s\n", received_msg);
        send_string_bt("Unknown command.\n");
    }
}

void state_change_password(char received_msg[128])
{
    received_msg[strcspn(received_msg, "\r\n")] = '\0';
    if (strlen(received_msg) == TAMANHO_SENHA && strspn(received_msg, "0123456789") == TAMANHO_SENHA)
    {
        for (int i = 0; i < TAMANHO_SENHA; i++)
        {
            senha[i] = received_msg[i] - '0'; // Converte char para inteiro
        }
        ESP_LOGI(SPP_TAG, "Senha alterada com sucesso: %s\n", received_msg);
        send_string_bt("Senha atualizada com sucesso!\n");
    }
    else
    {
        ESP_LOGI(SPP_TAG, "Senha inválida! Apenas dígitos são permitidos.\n");
        send_string_bt("Senha inválida! Certifique-se de usar apenas números.\n");
    }
}

void state_change_hora(char received_msg[128])
{
}

void state_change_data(char received_msg[128])
{
}

// Callback principal do SPP Bluetooth, aqui as coisas acontecem
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        // Evento de inicialização do SPP
        if (param->init.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%" PRIu32 " close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%" PRIu32 " sec_id:%d scn:%d", param->start.handle, param->start.sec_id,
                     param->start.scn);
            // esp_bt_gap_set_device_name(EXAMPLE_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        }
        else
        {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
        // Evento de recepção de dados
        // Todo o código desse arquivo é para apenas esse trecho abaixo funcionar, esse trecho basicamente é a main
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        // ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len:%d handle:%" PRIu32,
        //  param->data_ind.len, param->data_ind.handle);
        // if (param->data_ind.len < 128)
        // {
        // esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);
        // }
        bt_handle = param->data_ind.handle;
        param->data_ind.data[param->data_ind.len] = 0;
        param->data_ind.data[param->data_ind.len - 1] = 0;
        strcpy((char *)bt_buffer, (char *)param->data_ind.data);
        bt_data_flag = 1;

        char received_msg[128];

        if (get_string_bt(received_msg))
        {
            // maldito!
            received_msg[strcspn(received_msg, "\r\n")] = '\0';

            switch (current_state)
            {
            case STATE_RUNNING:
                state_running(received_msg);
                break;

            case STATE_CHANGE_DATA:
                state_change_data(received_msg);

                current_state = STATE_RUNNING;
                break;
            case STATE_CHANGE_PASSWORD:
                state_change_password(received_msg);
                current_state = STATE_RUNNING;
                break;
            case STATE_CHANGE_HORA:
                state_change_hora(received_msg);
                current_state = STATE_RUNNING;
                break;

            default:
                break;
            }
        }
        break;
#else
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3)
        {
            print_speed();
        }
#endif
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        // ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        // ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{

    switch (event)
    {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
    {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(SPP_TAG, "authentication success!");
        }
        else
        {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT:
    {
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit)
        {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        }
        else
        {
            ESP_LOGI(SPP_TAG, "Input pin code: XXXX");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %" PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%" PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        break;

    default:
    {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

void app_main(void)
{
    // inicializando memoria flash interna
    esp_err_t ret = nvs_flash_init();

    // Configura o GPIO do botão como entrada com pull - down
    gpio_config_t io_conf = {
        .pin_bit_mask = (GPIO_INPUT_PIN_SEL), // Define o pino do botão
        .mode = GPIO_MODE_INPUT,              // Configura como entrada
        .pull_up_en = GPIO_PULLUP_DISABLE,    // Desabilita o pull-up
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // Habilita o pull-down
        .intr_type = GPIO_INTR_DISABLE        // Sem interrupções
    };
    gpio_config(&io_conf);

    gpio_config_t output_conf = {
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&output_conf);

    // printf("Botões inicializados com sucesso!\n");

    // Verificando se a memoria flash interna foi inicializada com sucesso
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK)
    {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);
}
