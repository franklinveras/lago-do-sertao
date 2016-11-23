#include <Arduino.h>
#include <LCD.h>
#include <LiquidCrystal_SR.h>
#include <RotaryEncoder.h>
#include <TimerOne.h>
#include <SPI.h>
#include "RF24.h"
#include "nRF24L01.h"

/* ---------- Mapa de pinos do ATMEGA238P-PU ---------- */

const char LED_R_PIN = 3;
const char LED_G_PIN = 5;
const char LED_B_PIN = 6;

const char SR_DATA_PIN = 2;
const char SR_CLOCK_PIN = 4;
const char SR_LATCH_PIN = 7;

const char BT3_PIN= 8;
const char RF_CE_PIN = 9;
const char RF_CSN_PIN = 10;

const char LCD_DATA_PIN = A0;
const char LCD_CLOCK_PIN = A1;
const char LCD_ENABLE_PIN = A2;

const char LEITURAS_PIN = A3;
const char ENC_A_PIN = A5;
const char ENC_B_PIN = A4;



/* --------------- Mapa de pinos do 74HC595 ---------------- */
char LCD_BL_PIN = 1;
char RELE1_PIN = 2;
char RELE2_PIN = 4;
char RELE3_PIN = 8;
char RELE4_PIN = 16;
char LTA_PIN = 32;
char LTB_PIN = 64;
char LTC_PIN = 128;

/* --------- Estado inicial dos pinos do 74HC595 ----------- */
unsigned char estados=0;
boolean tmp_state = false;

boolean lcd_bl_state = false;
boolean rele1_state = false;
boolean rele2_state= false;
boolean rele3_state = false;
boolean rele4_state = false;
boolean lta_state = false;
boolean ltb_state = false;
boolean ltc_state= false;

/* ------- Altera o estado de um pino do 74HC595 -------*/
void srWrite(boolean &variavel, boolean valor)
{

    variavel=valor;

    estados =
        ((lcd_bl_state)?LCD_BL_PIN:0)+
        ((rele1_state)?RELE1_PIN:0)+
        ((rele2_state)?RELE2_PIN:0)+
        ((rele3_state)?RELE3_PIN:0)+
        ((rele4_state)?RELE4_PIN:0)+
        ((lta_state)?LTA_PIN:0)+
        ((ltb_state)?LTB_PIN:0)+
        ((ltc_state)?LTC_PIN:0);

    digitalWrite(SR_LATCH_PIN, LOW);
    shiftOut(SR_DATA_PIN, SR_CLOCK_PIN, MSBFIRST, estados);
    digitalWrite(SR_LATCH_PIN, HIGH);
}

int srAnalogRead(char pino)
{
    lta_state = (pino & 1);
    ltb_state = (pino & 2);
    ltc_state = (pino & 4);
    srWrite(lta_state, lta_state); /* Atualiza o 74HC595 para escolher o pino de leitura no 4051 */

    return analogRead(LEITURAS_PIN);
}

int srDigitalRead(char pino)
{
    lta_state = (pino & 1);
    ltb_state = (pino & 2);
    ltc_state = (pino & 4);

    srWrite(lta_state, lta_state);

    return digitalRead(LEITURAS_PIN);
}

/* ------------------- Mapa de pinos do 4051 ------------------- */
char INT1_PIN = 2;
char INT2_PIN = 1;
char INT3_PIN = 0;
char INT4_PIN = 3;
char BT1_PIN = 4;
char BT2_PIN = 5;
char LDR_PIN = 6;
char TEMP_PIN = 7;

boolean int4_def_state;
boolean int4_state;

/* ---------------  Ininicialização das classes ----------------- */
LiquidCrystal_SR lcd(LCD_DATA_PIN, LCD_CLOCK_PIN, LCD_ENABLE_PIN);
RotaryEncoder encoder(ENC_A_PIN, ENC_B_PIN);
RF24 radio(9,10);
/* ----------------- Informações do nRF24l01 -------------------- */
unsigned char radio_mensagem[2];

/* ------------- Informações do display 16x2 ---------------- */
int lcd_bl_off_time=20000;
unsigned long lcd_atividade=0;
char Cx, Cy = 0;

/* ----------------- Funções do display 16x2 ------------------ */
void lcdClear()
{
    lcd.clear();
    Cx=0;
    Cy=0;
    lcd.setCursor(Cx,Cy);
}

void lcdPrint(char msg[])
{

    char i=0;
    while(msg[i])
    {
        lcd.setCursor(Cx,Cy);
        lcd.print(msg[i]);
        Cx++;
        if (Cx>15)Cy++;
        Cy%=2;
        Cx%=16;
        i++;
    }

}

/* ----------- Retardo no acionamento do Filtro UV ---------- */
boolean uv_liga = false;
unsigned long uv_delay = 30000;
unsigned long uv_acionamento = 0;


/* --------------- Informações da fita LED RGB -------------- */
const char LED_MODO_DESLIGADO = 0;
const char LED_MODO_FADE = 1;
const char LED_MODO_STROBE = 2;

const char LED_MODO_BRANCO = 3;
const char LED_MODO_VERMELHO = 4;
const char LED_MODO_LARANJA = 5;
const char LED_MODO_AMARELO = 6;
const char LED_MODO_VERDE = 7;
const char LED_MODO_CIANO = 8;
const char LED_MODO_AZUL_CLARO = 9;
const char LED_MODO_AZUL = 10;
const char LED_MODO_ROXO = 11;
const char LED_MODO_MARGENTA = 12;
const char LED_MODO_PERSONALIZADO= 13;
char LED_MODOS[14][17]=
{
    "   Desligado    ",
    "      Fade      ",
    "     Strobe     ",
    "     Branco     ",
    "    Vermelho    ",
    "    Laranja     ",
    "    Amarelo     ",
    "     Verde      ",
    "     Ciano      ",
    "   Azul Claro   ",
    "      Azul      ",
    "      Roxo      ",
    "    Margenta    ",
    "  Personalizado "
};

char led_modo = LED_MODO_DESLIGADO;
unsigned char led_brilho=255;
unsigned char led_r=0;
unsigned char led_g=0;
unsigned char led_b=0;
unsigned char led_velocidade=50;

unsigned char fade_seq[3][12]
{
    {255,255,255,127,0  ,0  ,0  ,0  ,0  ,127,255,255},
    {0  ,0  ,127,255,255,255,255,127,0  ,0  ,0  ,0  },
    {0  ,0  ,0  ,0  ,0  ,127,255,255,255,255,255,127}
};

unsigned char cor_seq[3][14]
{
/*   -   -   -   Bra Ver Lar Ama Ver Cia AzC Azu Rox Mar Per */
    {0  ,0  ,0  ,255,255,255,255,0  ,0  ,0  ,0  ,127,255,0  },
    {0  ,0  ,0  ,255,0  ,35 ,75 ,255,255,127,0  ,0  ,0  ,0  },
    {0  ,0  ,0  ,255,0  ,0  ,0  ,0  ,255,255,255,255,255,0  }
};

unsigned char fade_tmp[3][12];
unsigned char cor_tmp[3][14];


/* ----------------- Funções da fita LED RGB ---------------- */
void ledCor(unsigned char R, unsigned char G,unsigned char B)
{
    analogWrite(LED_R_PIN,R);
    analogWrite(LED_G_PIN,G);
    analogWrite(LED_B_PIN,B);
}

void ledMudaVelocidade(unsigned char velocidade)
{
    Timer1.initialize(30000-290*velocidade);
}

void ledMudaBrilho(unsigned char brilho)
{
    for (int x = 0; x!=3; x++)
    {

        for (int y = 0; y!=12; y++)
        {
            fade_tmp[x][y]=map(fade_seq[x][y],0,255,0,brilho);
        }

        for (int y = 0; y!=14; y++)
        {
            cor_tmp[x][y]=map(cor_seq[x][y],0,255,0,brilho);
        }
    }
}


void ledStep()
{
    static int i = 0;
    static int j = 0;

    if (led_modo==LED_MODO_DESLIGADO)
    {
        digitalWrite(LED_R_PIN,LOW);
        digitalWrite(LED_G_PIN,LOW);
        digitalWrite(LED_B_PIN,LOW);
    }
    else
    {

        j = i / 255;

        if(j>11)
        {
            i=0;
            j=0;
        }

        switch(led_modo)
        {
        case LED_MODO_FADE:
            if(j<11)
            {
                led_r = map(i-(j*255),0,255,fade_tmp[0][j],fade_tmp[0][j+1]);
                led_g = map(i-(j*255),0,255,fade_tmp[1][j],fade_tmp[1][j+1]);
                led_b = map(i-(j*255),0,255,fade_tmp[2][j],fade_tmp[2][j+1]);
                ledCor(led_r,led_g,led_b);
            }
            if(j==11)
            {
                led_r = map(i-(j*255),0,255,fade_tmp[0][j],fade_tmp[0][0]);
                led_g = map(i-(j*255),0,255,fade_tmp[1][j],fade_tmp[1][0]);
                led_b = map(i-(j*255),0,255,fade_tmp[2][j],fade_tmp[2][0]);
                ledCor(led_r,led_g,led_b);
            }

            break;

        case LED_MODO_STROBE:
            ledCor(fade_tmp[0][j],fade_tmp[1][j],fade_tmp[2][j]);
            break;

        default:
            ledCor(cor_tmp[0][led_modo],cor_tmp[1][led_modo],cor_tmp[2][led_modo]);
            break;

        }
        i++;
    }
}


/* ----------------------- Eventos ---------------------------*/

char enc_offset=0;

unsigned long BT_debounce_time=50;

unsigned long BT1_ldbtime=0;
boolean BT1_state;
boolean BT1_lstate=false;

unsigned long BT2_ldbtime=0;
boolean BT2_state;
boolean BT2_lstate=true;

unsigned long BT3_ldbtime=0;
boolean BT3_state;
boolean BT3_lstate=true;

const char EV_NULL = 0;
const char EV_ENCODER_CHANGE = 1;

const char EV_BT1_PRESS = 2;
const char EV_BT2_PRESS = 3;
const char EV_BT3_PRESS = 4;

const char EV_INT4_CHANGE = 5;

const char EV_RADIO_DUMP_MODO_LED = 6;
const char EV_RADIO_SET_MODO_LED = 7;
const char EV_RADIO_DUMP_BRILHO = 8;
const char EV_RADIO_SET_BRILHO = 9;
const char EV_RADIO_DUMP_VELOCIDADE = 10;
const char EV_RADIO_SET_VELOCIDADE = 11;

const char EV_RADIO_DUMP_LED_VERMELHO = 12;
const char EV_RADIO_SET_LED_VERMELHO = 13;

const char EV_RADIO_DUMP_LED_VERDE = 14;
const char EV_RADIO_SET_LED_VERDE = 15;

const char EV_RADIO_DUMP_LED_AZUL = 16;
const char EV_RADIO_SET_LED_AZUL = 17;

const char EV_RADIO_DUMP_RELE1 = 18;
const char EV_RADIO_SET_RELE1 = 19;

const char EV_RADIO_DUMP_RELE2 = 20;
const char EV_RADIO_SET_RELE2 = 21;

const char EV_RADIO_DUMP_RELE3 = 22;
const char EV_RADIO_SET_RELE3 = 23;

const char EV_RADIO_DUMP_RELE4 = 24;
const char EV_RADIO_SET_RELE4 = 25;

const char EV_RADIO_DUMP_TEMP = 26;
const char EV_RADIO_DUMP_LDR_STATE = 27;

const unsigned char FB_POS = 255;
const unsigned char FB_NEG = 127;

char mn_menu = 0;
char mn_tmp_menu = 0;
char mn_submenu=0;
char mn_tmp_submenu = 0;
char mn_valor=0;

void lcdPrintEstado(boolean estado)
{
    (estado)?lcdPrint("    Ligado  "):lcdPrint("   Desligado");
}

void evento(char cod = 0)
{
    if(cod>5)
    {
        radio.stopListening();

        switch(cod){

            case EV_RADIO_DUMP_MODO_LED:
                radio_mensagem[1]=led_modo;
            break;

            case EV_RADIO_SET_MODO_LED:
                led_modo=radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_BRILHO:
                radio_mensagem[1] = led_brilho;
            break;

            case EV_RADIO_SET_BRILHO:
                led_brilho = radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_VELOCIDADE:
                radio_mensagem[1]=led_velocidade;
            break;

            case EV_RADIO_SET_VELOCIDADE:
                led_velocidade = radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_LED_VERMELHO:
                radio_mensagem[1]=cor_tmp[0][13];
            break;

            case EV_RADIO_SET_LED_VERMELHO:
                cor_tmp[0][13]=radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_LED_VERDE:
                radio_mensagem[1]=cor_tmp[1][13];
            break;

            case EV_RADIO_SET_LED_VERDE:
                cor_tmp[1][13]=radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_LED_AZUL:
                radio_mensagem[1]=cor_tmp[2][13];
            break;

            case EV_RADIO_SET_LED_AZUL:
                cor_tmp[2][13]=radio_mensagem[1];
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_RELE1:
                radio_mensagem[1]=rele1_state;
            break;

            case EV_RADIO_SET_RELE1:
                srWrite(rele1_state, radio_mensagem[1]);
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_RELE2:
                radio_mensagem[1]=rele2_state;
            break;

            case EV_RADIO_SET_RELE2:
                srWrite(rele2_state, radio_mensagem[1]);
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_RELE3:
                radio_mensagem[1]=rele3_state;
            break;

            case EV_RADIO_SET_RELE3:
                srWrite(rele3_state, radio_mensagem[1]);
                 radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_RELE4:
                radio_mensagem[1]=rele4_state;
            break;

            case EV_RADIO_SET_RELE4:
                srWrite(rele4_state, radio_mensagem[1]);
                radio_mensagem[1]=FB_POS;
            break;

            case EV_RADIO_DUMP_TEMP:
            break;

            case EV_RADIO_DUMP_LDR_STATE:
            break;

        }

        radio.write(&radio_mensagem, sizeof(unsigned char)*2);
        radio.startListening();

    }

    lcd_atividade=millis();
    mn_tmp_menu=mn_menu;
    mn_tmp_submenu=mn_submenu;

    if (cod == EV_INT4_CHANGE)
    {
        srWrite(rele4_state, !rele4_state);
    }
    /* ----------------------------- Menus ------------------------------*/
    switch(mn_menu)
    {
    case 0:
        switch(mn_submenu)
        {
        case 0:
            if(cod==EV_NULL)
            {
                lcdClear();
                lcdPrint(" Lago do Sertao     Ver 1.1");
            }
            if (cod == EV_BT1_PRESS)
            {
                mn_menu=1;
                mn_submenu=0;
            }
            if (cod == EV_BT2_PRESS)
            {
                mn_menu=2;
                mn_submenu=2;
                mn_valor=1;
            }
            if (cod == EV_ENCODER_CHANGE)
            {
                mn_menu=2;
                mn_submenu=1;
                mn_valor=1;
            }
            break;
        }
        break;

    case 1:
        switch(mn_submenu)
        {
        case 0:
            if(cod==EV_NULL)
            {
                lcdClear();
                lcdPrint(" Liga / Desliga      Tomada");
            }

            if (cod == EV_BT2_PRESS)
            {
                mn_submenu++;
            }
            if (cod == EV_BT3_PRESS)
            {
                mn_menu--;
            }
            if (cod == EV_ENCODER_CHANGE)
            {
                if(enc_offset>0)mn_menu++;
            }
            break;

        case 1:

            if(mn_valor==0)
            {
                tmp_state=rele1_state;
                if(cod==EV_NULL)
                {
                    lcdClear();
                    lcdPrint("   Bomba de        Filtragem");
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    if(enc_offset>0)
                    {
                        mn_submenu=2;
                    }
                }

                if (cod == EV_BT2_PRESS)
                {
                    mn_valor=1;
                }

                if (cod == EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }
            }
            else
            {
                lcdClear();
                lcdPrint("B. de Filtragem ");


                if (cod == EV_ENCODER_CHANGE)
                {

                    if(enc_offset>0)
                    {
                        srWrite(rele1_state,true);
                        uv_acionamento=millis();
                    }
                    else
                    {
                        srWrite(rele1_state,false);
                        srWrite(rele2_state,false);
                        uv_liga=false;
                    }
                }
                Cx=0;
                Cy=1;
                lcd.setCursor(Cx,Cy);
                lcdPrintEstado(rele1_state);

                if(cod == EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod == EV_BT3_PRESS)
                {
                    mn_valor=0;
                    srWrite(rele1_state, tmp_state);
                }
            }
            break;

        case 2:
            if(mn_valor==0)
            {
                tmp_state=rele2_state;
                if(cod==EV_NULL)
                {
                    lcdClear();
                    lcdPrint("     Filtro       Ultravioleta");
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    mn_submenu+=enc_offset;
                }

                if (cod == EV_BT2_PRESS)
                {
                    mn_valor=2;
                }
                if (cod == EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }

            }
            else
            {
                lcdClear();
                lcdPrint("   Filtro UV");

                if (cod == EV_ENCODER_CHANGE)
                {

                    if(enc_offset>0)
                    {
                        if(rele1_state && (millis()-uv_acionamento>uv_delay))
                        {
                            uv_liga=true;
                        }
                    }
                    else
                    {
                        srWrite(rele2_state,false);
                    }
                }

                Cx=0;
                Cy=1;
                lcd.setCursor(Cx,Cy);
                lcdPrintEstado(uv_liga||rele2_state);

                if(cod == EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod == EV_BT3_PRESS)
                {
                    mn_valor=0;
                    srWrite(rele2_state, tmp_state);
                }
            }
            break;

        case 3:
            if(mn_valor==0)
            {
                if(cod==EV_NULL)
                {
                    lcdClear();
                    lcdPrint("    Cascata");
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    mn_submenu+=enc_offset;
                }

                if (cod == EV_BT2_PRESS)
                {
                    mn_valor=3;
                }
                if (cod == EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }

            }
            else
            {
                lcdClear();
                lcdPrint("    Cascata");

                if (cod == EV_NULL)
                {
                    tmp_state=rele3_state;
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    if(enc_offset>0)
                    {
                        srWrite(rele3_state,true);
                    }
                    else
                    {
                        srWrite(rele3_state,false);
                    }
                }

                Cx=0;
                Cy=1;
                lcd.setCursor(Cx,Cy);
                lcdPrintEstado(rele3_state);

                if(cod == EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod == EV_BT3_PRESS)
                {
                    mn_valor=0;
                    srWrite(rele3_state, tmp_state);
                }
            }
            break;

        case 4:
            if(mn_valor==0)
            {
                if(cod==EV_NULL)
                {
                    lcdClear();
                    lcdPrint("   Iluminacao       Externa");
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    if(enc_offset<0)
                    {
                        mn_submenu=3;
                    }
                }
                if (cod == EV_BT2_PRESS)
                {
                    mn_valor=4;
                }
                if (cod == EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }
            }
            else
            {
                lcdClear();
                lcdPrint("  Ilum Externa");

                if (cod == EV_NULL)
                {
                    tmp_state=rele1_state;
                }

                if (cod == EV_ENCODER_CHANGE)
                {

                    if(enc_offset>0)
                    {
                        srWrite(rele4_state,true);
                    }
                    else
                    {
                        srWrite(rele4_state,false);
                    }
                }

                Cx=0;
                Cy=1;
                lcd.setCursor(Cx,Cy);
                lcdPrintEstado(rele4_state);

                if(cod == EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod == EV_BT3_PRESS)
                {
                    mn_valor=0;
                    srWrite(rele4_state, tmp_state);
                }
            }
            break;

        }

        break;
    case 2:
        switch(mn_submenu)
        {
        case 0:
            lcdClear();
            lcdPrint("  Configuracoes      de LED");

            if (cod == EV_BT2_PRESS)
            {
                mn_submenu++;
            }
            if (cod == EV_BT3_PRESS)
            {
                mn_menu=0;
            }
            if (cod == EV_ENCODER_CHANGE)
            {
                if(enc_offset<0)mn_menu--;
            }
            break;
        case 1:
            static unsigned char tmp_led_modo=0;
            if(cod==EV_NULL)
            {
                lcdClear();
                lcdPrint("  Modo do Led   ");
            }
            switch(mn_valor)
            {

            case 0:
                tmp_led_modo=led_modo;

                if(cod==EV_ENCODER_CHANGE)
                {
                    if(enc_offset>0)mn_submenu++;
                }

                if (cod == EV_BT2_PRESS)
                {
                    mn_valor++;
                }

                if (cod == EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }
                break;

            case 1:
                if(cod == EV_NULL)
                {
                    lcdPrint(LED_MODOS[led_modo]);
                }

                if(cod == EV_BT2_PRESS)
                {
                    if(led_modo==13)
                    {
                        mn_valor=2;
                    }
                    else
                    {
                        mn_valor=0;
                    }
                }

                if(cod == EV_BT3_PRESS)
                {
                    mn_valor=0;
                    led_modo=tmp_led_modo;
                }

                if (cod == EV_ENCODER_CHANGE)
                {
                    switch (led_modo)
                    {
                    case 0:
                        if(enc_offset>0)led_modo++;
                        break;

                    case 13:
                        if(enc_offset<0)led_modo--;
                        break;

                    default:
                        led_modo+=enc_offset;
                        break;
                    }
                }
                break;
            case 2:
                static unsigned char tmp_cor=0;
                static char opt=0;
                tmp_cor=cor_tmp[opt][13];
                if(cod==EV_NULL)
                {
                    lcdClear();
                    lcdPrint("Verm Verde Azul");
                    for(int i = 0; i<3; i++)
                    {
                        Cx=1+i*5;
                        Cy=1;
                        lcd.setCursor(Cx,Cy);
                        lcd.print(cor_tmp[i][13]);
                    }
                    lcd.setCursor(opt*5,Cy);
                    lcd.blink();

                }
                if(cod==EV_ENCODER_CHANGE)
                {
                    switch(tmp_cor)
                    {
                    case 0:
                        if(enc_offset>0)tmp_cor++;
                        break;
                    case 255:
                        if(enc_offset<0)tmp_cor--;
                        break;
                    default:
                        tmp_cor+=enc_offset;
                        break;

                    }
                    cor_tmp[opt][13]=tmp_cor;
                }
                if(cod==EV_BT2_PRESS)
                {
                    if(opt<3)
                    {
                        tmp_cor=0;
                        opt++;
                    }
                    if(opt==3)
                    {
                        for(int i = 0; i<3; i++) cor_seq[i][13]=cor_tmp[i][13];
                        mn_valor=0;
                        opt=0;
                        lcd.noBlink();
                    }
                }
                if(cod==EV_BT3_PRESS)
                {
                    if(opt==0)
                    {
                        tmp_cor=0;
                        mn_valor--;
                        lcd.noBlink();
                        for(int i = 0; i<3; i++) cor_tmp[i][13]=0;
                    }
                    if(opt>0)
                    {
                        opt--;
                    }
                }
                break;
            }
            break;
        case 2:
            unsigned static char tmp_led_brilho;
            lcdClear();
            lcdPrint("     Brilho");
            if (mn_valor==0)
            {
                tmp_led_brilho=led_brilho;
                if(cod==EV_ENCODER_CHANGE)
                {
                    mn_submenu+=enc_offset;
                }

                if(cod==EV_BT2_PRESS)
                {
                    mn_valor=1;
                }

                if(cod==EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }
            }
            else
            {
                lcd.setCursor(6,1);
                lcd.print(led_brilho);
                if(cod==EV_ENCODER_CHANGE)
                {
                    switch(led_brilho)
                    {

                    case 0:
                        if(enc_offset>0)led_brilho++;
                        break;

                    case 255:
                        if(enc_offset<0)led_brilho--;
                        break;

                    default:
                        led_brilho+=enc_offset;
                        break;
                    }
                    ledMudaBrilho(led_brilho);
                }
                if(cod==EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod==EV_BT3_PRESS)
                {
                    led_brilho=tmp_led_brilho;
                    ledMudaBrilho(led_brilho);
                    mn_valor=0;
                }
            }
            break;
        case 3:
            unsigned static char tmp_led_velocidade;
            lcdClear();
            lcdPrint("   Velocidade");
            if (mn_valor==0)
            {
                tmp_led_velocidade=led_velocidade;

                if(cod==EV_ENCODER_CHANGE)
                {
                    if(enc_offset<0)mn_submenu--;
                }

                if(cod==EV_BT2_PRESS)
                {
                    mn_valor=1;
                }

                if(cod==EV_BT3_PRESS)
                {
                    mn_submenu=0;
                }
            }
            else
            {
                lcd.setCursor(6,1);
                lcd.print(led_velocidade);
                if(cod==EV_ENCODER_CHANGE)
                {
                    switch(led_velocidade)
                    {

                    case 0:
                        if(enc_offset>0)led_velocidade++;
                        break;

                    case 100:
                        if(enc_offset<0)led_velocidade--;
                        break;

                    default:
                        led_velocidade+=enc_offset;
                        break;
                    }
                    ledMudaVelocidade(led_velocidade);
                }
                if(cod==EV_BT2_PRESS)
                {
                    mn_valor=0;
                }

                if(cod==EV_BT3_PRESS)
                {
                    led_velocidade=tmp_led_velocidade;
                    ledMudaVelocidade(led_velocidade);
                    mn_valor=0;
                }
            }
            break;
        }
        break;
    }

    if(cod!=0)
    {
        evento(EV_NULL);
        srWrite(lcd_bl_state, true);
    }
}

void setup ( )

{
    ledMudaBrilho(led_brilho);
    ledMudaVelocidade(led_velocidade);
    Timer1.attachInterrupt(ledStep);

    Serial.begin(9600);
    lcd.begin ( 16, 2 );

/*  Ajusta os parâmetros do Transceptor nRF24l01+  */
    radio.begin();
    radio.setRetries(15,15);
    radio.openReadingPipe(1,0xF0F0F0F0BA);
    radio.openWritingPipe(0xF0F0F0F0AB);
    radio.startListening();

    evento(EV_NULL);

    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    pinMode(SR_DATA_PIN, OUTPUT);
    pinMode(SR_CLOCK_PIN, OUTPUT);
    pinMode(SR_LATCH_PIN, OUTPUT);
    pinMode(BT3_PIN, INPUT);
    pinMode(LEITURAS_PIN, INPUT);

    srWrite(lcd_bl_state, true);

    /*  Acionamento automático das cargas */
    srWrite(rele1_state, true);
    uv_liga=true;
    srWrite(rele3_state, true);
    srWrite(rele4_state, true);
    int4_def_state = srDigitalRead(INT4_PIN);

}

void loop ()
{

    /* ----------------------------- Leituras ------------------------------*/

    /*  Leitura do encoder */
    while(radio.available())
    {
        radio.read(&radio_mensagem, sizeof(unsigned char) * 2);
        evento(radio_mensagem[0]);
    }

    /*  Leitura do encoder */
    static int enc_pos = 0;
    encoder.tick();
    int enc_new_pos = encoder.getPosition();

    if (enc_pos != enc_new_pos)
    {
        enc_offset = enc_new_pos - enc_pos;
        evento(EV_ENCODER_CHANGE);
        enc_pos = enc_new_pos;
    }

    /*  Leitura do botão de Menu */
    boolean reading = (srDigitalRead(BT1_PIN))?0:1;
    if(BT1_lstate!=reading)
    {
        BT1_ldbtime=millis();
    };

    if(millis()-BT1_ldbtime>BT_debounce_time)
    {
        if (BT1_state!=reading)
        {
            BT1_state = reading;
            if(BT1_state == false)
            {
                evento(EV_BT1_PRESS);
            }
        }

    }
    BT1_lstate = reading;


    /*  Leitura do botão Confirmar */
    reading = (srDigitalRead(BT2_PIN))?0:1;
    if(BT2_lstate!=reading)
    {
        BT2_ldbtime=millis();
    };

    if(millis()-BT2_ldbtime>BT_debounce_time)
    {
        if (BT2_state!=reading)
        {
            BT2_state = reading;
            if(BT2_state == false)
            {
                evento(EV_BT2_PRESS);
            }
        }

    }
    BT2_lstate = reading;

    /*  Leitura do botão Cancelar */
    reading = (digitalRead(BT3_PIN))?0:1;
    if(BT3_lstate!=reading)
    {
        BT3_ldbtime=millis();
    };

    if(millis()-BT3_ldbtime>BT_debounce_time)
    {
        if (BT3_state!=reading)
        {
            BT3_state = reading;
            if(BT3_state == false)
            {
                evento(EV_BT3_PRESS);
            }
        }

    }
    BT3_lstate = reading;

    /*  Leitura do interruptor da iluminação externa */
    int4_state = srDigitalRead(INT4_PIN);
    if (int4_def_state!=int4_state)
    {
        int4_def_state=int4_state;
        evento(EV_INT4_CHANGE);
    }

    /* Desliga a luz de fundo do display ao atingir o tempo limite */
    if (millis() - lcd_atividade>lcd_bl_off_time && lcd_bl_state)
    {
        srWrite(lcd_bl_state, false);
        mn_menu=0;
        mn_submenu=0;
        mn_valor=0;
        lcd.noBlink();
        evento(EV_NULL);
    }

    /* Liga o filtro UV ao atingir o tempo limite */
    if ((millis() - uv_acionamento>uv_delay) && uv_liga)
    {
        srWrite(rele2_state, true);
        uv_liga=false;
    }

}
