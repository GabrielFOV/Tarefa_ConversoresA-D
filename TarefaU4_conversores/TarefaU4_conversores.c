#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "auxiliar/ssd1306.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"      
#include "hardware/pwm.h"

//Definição dos pinos.
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define UART_ID uart0 // Seleciona a UART0.
#define BAUD_RATE 115200 // Define a taxa de transmissão.
#define UART_TX_PIN 0 // Pino GPIO usado para TX.
#define UART_RX_PIN 1 // Pino GPIO usado para RX.
#define LED1_PIN 11 // Led azul.
#define LED2_PIN 13 // Led vermelho.
#define LED3_PIN 12 // Led verde.
#define VRX_PIN 27 
#define VRY_PIN 26 
#define SW_PIN 22 


//Configurações do botâo.
#define tempo 2500
const uint button_A = 5; 
uint32_t current_time;

//Variáveis globais.
static volatile uint a = 1;
ssd1306_t ssd; //Inicializa a estrutura do display.
bool cor = true; 
bool estado = true;
int grosso = 3;
int fino = 1;
int espessura = 1;
bool est = true;
static volatile uint32_t last_time_A = 0; //Armazena o tempo do último evento (em microssegundos).
static volatile uint32_t last_time_SW = 0;//Armazena o tempo do último evento (em microssegundos). 
static volatile bool estado_led3 = false; //Armazena estado do led verde.

//Prototipação da função de interrupção.
static void gpio_irq_handlerA(uint gpio, uint32_t events);

//Definição de PWM. 
#define WRAP 20000 // Para 50Hz (20ms) frequência PWM.
#define Divisor  125  // Divisor de clock ajustado para servo.

void pwm_setup()//Função de inicialização para o PWM. 
{
    gpio_set_function(LED2_PIN, GPIO_FUNC_PWM); // Configura GPIO 13 como PWM.
    uint slice_red = pwm_gpio_to_slice_num(LED2_PIN); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_red, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_red, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_red, true);       // Habilita o PWM.

    gpio_set_function(LED1_PIN, GPIO_FUNC_PWM); // Configura GPIO 11 como PWM.
    uint slice_blue = pwm_gpio_to_slice_num(LED1_PIN); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_blue, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_blue, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_blue, true);       // Habilita o PWM.
}

// Função para definir a luminosidade do led vermelho.
void Controle_azul(uint16_t direcao) 
{
    if(estado==true)//Reconhece o acinamento do botão A.
  {
    pwm_set_gpio_level(LED2_PIN, direcao);//Configura a intensidade da luz.
  }
  else
  {
    pwm_set_gpio_level(LED2_PIN, 0);//Mantem desligado se o botão foi pressionado.
  }
}

// Função para definir a luminosidade do led azul.
void Controle_vermelho(uint16_t direcao) 
{
  if(estado==true)//Reconhece o acinamento do botão A.
  {
    pwm_set_gpio_level(LED1_PIN, direcao); //Configura a intensidade da luz.
  }
  else
  {
    pwm_set_gpio_level(LED1_PIN, 0);//Mantem desligado se o botão foi pressionado.
  }
}

// Controla o estado do led verde. 
void Controle_verde(bool estado_led3)
{
    if(estado==true)//Confere se o botão A foi acionado.
    {
        if (estado_led3)//Confere se o botão do joystick foi pressionado. 
        {
            gpio_put(LED3_PIN, true);  // Liga o LED
        } 
        else 
        {
            gpio_put(LED3_PIN, false); // Desliga o LED
        }
    }
    else
    {
        gpio_put(LED3_PIN, false); // Desliga o LED
    }
}

//Regula a espessura da borda do display.
void espessura_borda(bool esp)
{
    if(esp==true)
    {
        espessura = fino;
    }
    else
    {
        espessura = grosso; 
    }

}

int main()
{
    //Inicializa os periféricos para uso de funções c padrão.
    stdio_init_all();

    //I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    //Inicializa a UART.
    uart_init(UART_ID, BAUD_RATE);

    //Configura os pinos GPIO para a UART.
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); // Configura o pino 0 para TX
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART); // Configura o pino 1 para RX

    //Inicializa o adc.
    adc_init();
    adc_gpio_init(VRX_PIN); 
    adc_gpio_init(VRY_PIN); 

    //Inicializa o gpio para o botão do joystick.
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);//Configura o pino como entrada.
    gpio_pull_up(SW_PIN); //Habilita o pull-up interno.

    //Inicializa o led azul
    gpio_init(LED1_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_put(LED1_PIN, false); 

    //Inicializa o led vermelho.
    gpio_init(LED2_PIN);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_put(LED2_PIN, false); 

    //Inicializa o led verde.
    gpio_init(LED3_PIN);
    gpio_set_dir(LED3_PIN, GPIO_OUT);
    gpio_put(LED3_PIN, false);

    //Inicializa o botão A.
    gpio_init(button_A);
    gpio_set_dir(button_A, GPIO_IN); //Configura o pino como entrada.
    gpio_pull_up(button_A);          //Habilita o pull-up interno.
    
    //Inicializa o display.
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); //Configura o pino GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino GPIO para I2C
    gpio_pull_up(I2C_SDA); //Habilita o pull up para os dados
    gpio_pull_up(I2C_SCL); //Habilita o pull up para o clock
  
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); //Inicializa o display.
    ssd1306_config(&ssd); //Configura o display.
    ssd1306_send_data(&ssd); //Envia os dados para o display.

    //Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    pwm_setup();//Inicializa o PWM.

    //Função de interrupção. 
    gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handlerA);
    gpio_set_irq_enabled(SW_PIN, GPIO_IRQ_EDGE_FALL, true);

  while (true)
  {
        //Configura o ADC para os eixos X e Y.
        adc_select_input(0); 
        uint16_t vrx_value = adc_read(); 
        adc_select_input(1); 
        uint16_t vry_value = adc_read(); 
        bool sw_value = gpio_get(SW_PIN) == 0; 

        ssd1306_fill(&ssd, !cor); //Limpa o display.
        ssd1306_rect(&ssd, 3, 3, 120, 56, cor, !cor, espessura); //Desenha a borda.
        ssd1306_send_data(&ssd); //Atualiza o display.
        if (vrx_value > 2047) 
        {
            Controle_vermelho(vrx_value*5);//Ajusta o brilho.
           sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
        } 
        if((vrx_value < 2047))
        {
            Controle_vermelho((4095-vrx_value)*5);//Ajusta o brilho.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
        }
        if (vry_value > 2047) 
        {
            Controle_azul(vry_value*5);//Ajusta o brilho.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
        } 
        if(vry_value < 2047)
        {
            Controle_azul((4095-vry_value)*5);//Ajusta o brilho.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança 
        }
        if((vry_value == 2047)&&(vrx_value == 2047))
        {
            Controle_azul(0);
            Controle_vermelho(0);
            ssd1306_square(&ssd, 30, 60, 8, 8, cor, cor); //Desenha um quadrado.
            ssd1306_send_data(&ssd); //Atualiza o display.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
        }else
        {
            ssd1306_square(&ssd, (((4095-vrx_value)/79)+2), (((4095-vry_value)/35)+2), 8, 8, cor, cor); //Desenha um retângulo.
            ssd1306_send_data(&ssd); //Atualiza o display.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança. 
        }
        printf("VRX: %u, VRY: %u, SW: %d\n", vrx_value, vry_value, sw_value);//imprime valores do ADC.
        sleep_ms(100);
  }
  
}

//Função de interrupção com debouncing.
void gpio_irq_handlerA(uint gpio, uint32_t events) {
    current_time = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos

    if (gpio == button_A)// Se a interrupção veio do botão A
    { 
        if (current_time - last_time_A > 200000) { // 200 ms de debouncing
            last_time_A = current_time; // Atualiza o tempo do último evento

            printf("Mudança de Estado dos LEDs. A = %d\n", a);

            estado = !estado; // Alterna o estado do LED
            //Controle_verde(estado); // Controla o LED Verde
            
            // Exibir estado via UART
            if (estado) {
                uart_puts(UART_ID, "LED ativado\r\n");
            } else {
                uart_puts(UART_ID, "LED desativado\r\n");
                gpio_put(LED3_PIN, false);
            }

            a++; // Incrementa contador de eventos
        }
    }
    if (gpio == SW_PIN) // Se a interrupção veio do botão SW
    { 
        if (current_time - last_time_SW > 200000) // 200 ms de debouncing
        { 
            last_time_SW = current_time; // Atualiza o tempo do último evento
            est = !est; // Alterna o estado de espessura
            espessura_borda(est);
            // Alterna o estado do LED apenas se est == true
            if (est) 
            {
                estado_led3 = !estado_led3; // Alterna manualmente o estado do LED
            } 
            else 
            {
                estado_led3 = false; // Mantém desligado se est for false
            }
            Controle_verde(estado_led3); // Aplica a mudança no LED verde.
        }
    }
}