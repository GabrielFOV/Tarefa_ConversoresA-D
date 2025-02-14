
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "auxiliar/ssd1306.h"
#include "auxiliar/font.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/adc.h"      
#include "hardware/pwm.h"

//Biblioteca gerada pelo arquivo .pio durante compilação.


#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define UART_ID uart0 // Seleciona a UART0.
#define BAUD_RATE 115200 // Define a taxa de transmissão.
#define UART_TX_PIN 0 // Pino GPIO usado para TX.
#define UART_RX_PIN 1 // Pino GPIO usado para RX.
#define LED1_PIN 11  
#define LED2_PIN 13  
#define LED3_PIN 12  
#define VRX_PIN 26   
#define VRY_PIN 27   
#define SW_PIN 22 

//Definição do número de LEDs e pino.
#define LED_COUNT 25
#define LED_PIN 7

//Configurações dos pinos.
const uint button_A = 5; // Botão B = 6.
const uint button_B = 6; // Botão A = 5.
#define tempo 2500

uint32_t current_time;

//Variáveis globais.
static volatile uint a = 1;
static volatile uint b = 1;
static volatile uint32_t last_time = 0; //Armazena o tempo do último evento (em microssegundos).
char apresentar[2];
char caracter;
ssd1306_t ssd; //Inicializa a estrutura do display.
bool cor = true; 
bool estado = true;
int grosso = 3;
int fino = 1;
int espessura = 1;
bool est = true;

//Prototipação da função de interrupção.
static void gpio_irq_handlerA(uint gpio, uint32_t events);

//#define servo_pin 12   // GPIO 22 conectado ao servo.
#define WRAP 20000 // Para 50Hz (20ms) frequência PWM.
#define angulo_0   500   // 0° - Duty cycle de 500 microssegundos.
#define angulo_90  1470  // 90° - Duty cycle de 1475 microssegundos.
#define angulo_180 2400  // 180° - Duty cycle de 2400 microssegundos.
#define Divisor  125  // Divisor de clock ajustado para servo.

void pwm_setup()//Função de inicialização para o PWM. 
{
    gpio_set_function(LED2_PIN, GPIO_FUNC_PWM); // Configura GPIO 22 como PWM.
    uint slice_red = pwm_gpio_to_slice_num(LED2_PIN); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_red, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_red, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_red, true);       // Habilita o PWM.

    gpio_set_function(LED1_PIN, GPIO_FUNC_PWM); // Configura GPIO 22 como PWM.
    uint slice_blue = pwm_gpio_to_slice_num(LED1_PIN); // Obtém o slice do PWM.
    pwm_set_clkdiv(slice_blue, Divisor); // Define divisor de clock.
    pwm_set_wrap(slice_blue, WRAP);   // Define período do PWM (50Hz).
    pwm_set_enabled(slice_blue, true);       // Habilita o PWM.
}

// Função para definir o ângulo do servo (0° a 180°).
void Controle_vermelho(uint16_t direcao) 
{
    if(estado==true)
  {
    pwm_set_gpio_level(LED2_PIN, direcao);
  }
  else
  {
    pwm_set_gpio_level(LED2_PIN, 0);
  }
}

// Função para definir o ângulo do servo (0° a 180°).
void Controle_azul(uint16_t direcao) 
{
  if(estado==true)
  {
    pwm_set_gpio_level(LED1_PIN, direcao);
  }
  else
  {
    pwm_set_gpio_level(LED1_PIN, 0);
  }
}

void Controle_verde( bool est) 
{
    if (est) //200 ms de debouncing.
    {
        
        // Verifica se o botão está pressionado
        if (gpio_get(SW_PIN) == false) { 
            gpio_put(LED3_PIN, !gpio_get(LED3_PIN)); // Alterna o LED
        }
    } else 
    {
        // Se est for falso, desliga o LED e ignora o botão
        gpio_put(LED3_PIN, false);
    }
}

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

  //Mensagem inicial.
  const char *init_message = "UART Demo - RP2\r\n"
                              "Digite algo e veja o eco:\r\n";
  uart_puts(UART_ID, init_message);

  //Inicializa os leds.
  adc_init();

    adc_gpio_init(VRX_PIN); 
    adc_gpio_init(VRY_PIN); 

    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN); 

    gpio_init(LED1_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_put(LED1_PIN, false); 

    gpio_init(LED2_PIN);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_put(LED2_PIN, false); 

    gpio_init(LED3_PIN);
    gpio_set_dir(LED3_PIN, GPIO_OUT);
    gpio_put(LED3_PIN, false);

  //Inicializa os botôes.
  gpio_init(button_A);
  gpio_set_dir(button_A, GPIO_IN); //Configura o pino como entrada.
  gpio_pull_up(button_A);          //Habilita o pull-up interno.
  gpio_init(button_B);
  gpio_set_dir(button_B, GPIO_IN); //Configura o pino como entrada.
  gpio_pull_up(button_B);          //Habilita o pull-up interno.


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
        adc_select_input(0); 
        uint16_t vrx_value = adc_read(); 
        adc_select_input(1); 
        uint16_t vry_value = adc_read(); 
        bool sw_value = gpio_get(SW_PIN) == 0; 

        ssd1306_fill(&ssd, !cor); //Limpa o display.
        ssd1306_rect(&ssd, 3, 3, 122, 56, cor, !cor, espessura); //Desenha um retângulo.
        ssd1306_send_data(&ssd); //Atualiza o display.
        if (vrx_value > 2047) 
        {
            Controle_vermelho(vrx_value*5);//Ajusta o angulo.
           sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        } 
        if((vrx_value < 2047))
        {
            Controle_vermelho((4095-vrx_value)*5);
       
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        }
        if (vry_value > 2047) 
        {
            Controle_azul(vry_value*5);//Ajusta o angulo.
      
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        } 
        if(vry_value < 2047)
        {
            Controle_azul((4095-vry_value)*5);
      
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        }
        if((vry_value == 2047)&&(vrx_value == 2047))
        {
            Controle_azul(0);
            Controle_vermelho(0);
            ssd1306_square(&ssd, 30, 60, 8, 8, cor, cor); //Desenha um retângulo.
            ssd1306_send_data(&ssd); //Atualiza o display.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        }else{
         ssd1306_square(&ssd, (((4095-vrx_value)/79)+2), (((4095-vry_value)/35)+2), 8, 8, cor, cor); //Desenha um retângulo.
            ssd1306_draw_string(&ssd, apresentar, 60, 30); //Desenha uma string.
            ssd1306_send_data(&ssd); //Atualiza o display.
            sleep_ms(10);//Aguarda 10 microssegundos para a próxima mudança de angulo. 
        }
        printf("VRX: %u, VRY: %u, SW: %d\n", vrx_value, vry_value, sw_value);
        printf("%d\n",espessura);
        sleep_ms(100);
   
  }
  
}

//Função de interrupção com debouncing.
void gpio_irq_handlerA(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos.
    current_time = to_us_since_boot(get_absolute_time());
    //Verifica se passou tempo suficiente desde o último evento.
    //Diferencia o botão A do B.
    if (gpio == button_A && (current_time - last_time > 200000)) //200 ms de debouncing.
    {
        last_time = current_time; //Atualiza o tempo do último evento.
        printf("Mudanca de Estado dos Leds. A = %d\n", a);
        //gpio_put(button_A, !gpio_get(button_A)); //Alterna o estado.
        if(gpio_get(button_A)==false)//Reconhece o estado do led.
        {
          uart_puts(UART_ID, " led desativado\r\n");

          estado=false;
        } else { uart_puts(UART_ID, " led ativado\r\n");
          estado=true;
        }
        Controle_verde(estado);
        a++;//Incrementa a variavel de verificação
    }
     if (gpio == SW_PIN && (current_time - last_time > 200000)) //200 ms de debouncing.
    {
        last_time = current_time; //Atualiza o tempo do último evento.
        printf("Mudanca de Estado dos Leds. A = %d\n", a);
        Controle_verde(estado);
        est=(!est);
        espessura_borda(est);
        a++;//Incrementa a variavel de verificação
    }
}
