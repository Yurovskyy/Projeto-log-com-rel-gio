#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
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

#define SPP_TAG "LOGS"            // Tag para logs
#define SPP_SERVER_NAME "YURIESP" // Nome do servidor bluetooth
#define SPP_SHOW_DATA 0
// #define SPP_SHOW_SPEED 1
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

// Variavel para mudar a senha
bool aguardando = false;

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

// Callback principal do SPP
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
            if (aguardando)
            {
                printf("teste %s\n", received_msg);
                received_msg[strcspn(received_msg, "\r\n")] = '\0';
                printf("teste %s\n", received_msg);
                if (strlen(received_msg) == TAMANHO_SENHA && strspn(received_msg, "0123456789") == TAMANHO_SENHA)
                {
                    for (int i = 0; i < TAMANHO_SENHA; i++)
                    {
                        senha[i] = received_msg[i] - '0'; // Converte char para inteiro
                    }
                    ESP_LOGI(SPP_TAG, "Senha alterada com sucesso: %s\n", received_msg);
                    send_string_bt("Senha atualizada com sucesso!\n");
                    aguardando = false;
                }
                else
                {
                    ESP_LOGI(SPP_TAG, "Senha inválida! Apenas dígitos são permitidos.\n");
                    send_string_bt("Senha inválida! Certifique-se de usar apenas números.\n");
                    aguardando = false;
                }
            }
            else if (strcmp(received_msg, "senha") == 0)
            {
                ESP_LOGI(SPP_TAG, "Digite os 6 dígitos da nova senha:\n");
                send_string_bt("Digite os 6 dígitos da nova senha:\n");
                aguardando = true;
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
                            printf("BUFFER PARA DATA E HORA ATUAL\n");
                            send_string_bt("BUFFER PARA DATA E HORA ATUAL\n");
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
            else
            {
                ESP_LOGW(SPP_TAG, "Unknown command: %s\n", received_msg);
                send_string_bt("Unknown command.\n");
            }
        }
        break;
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
