#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Keypad.h>
#include <HardwareSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SIM_RX 16
#define SIM_TX 17
#define RING_PIN 19

HardwareSerial SIM800(1);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Adafruit_Keypad Clavier = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const String CHIFFRES = "0123456789";

volatile bool incomingCall = false;
volatile bool ongoingCall = false;
volatile bool outgoingCall = false;

String phoneNumber = "";
String oldPhoneNumber = "";
String incomingCallNumber = "";

void IRAM_ATTR ringISR();
void updateDisplayWhileCalling(bool d = false);
void updateDisplayWhileInputingNumber();
void parseCLCCResponse(String response);
void getCurrentCallingNumber();
void makeCall();
void hangUp();
void receiveCall();
void setupSim();
void setupScreen();

void setup()
{
    Serial.begin(115200);

    setupScreen();

    setupSim();

    Clavier.begin();
}

void loop()
{
    Clavier.tick();

    while (Clavier.available())
    {
        keypadEvent e = Clavier.read();

        if (e.bit.EVENT == KEY_JUST_PRESSED)
        {
            char key = e.bit.KEY;
            Serial.println(key);

            bool callEvent = outgoingCall || incomingCall || ongoingCall;

            // ACTIONS POSSIBLES SEULEMENT SI PAS D'APPEL EN COURS
            if (!callEvent)
            {
                // AJOUT D'UN CHIFFRE AU NUMERO
                if (CHIFFRES.indexOf(key) != -1)
                {
                    phoneNumber += key;
                    updateDisplayWhileInputingNumber();
                }

                // SUPPRIMER LE DERNIER CHIFFRE DU NUMERO SI POSSIBLE
                if (key == 'B')
                {
                    if (phoneNumber.length() > 0)
                    {
                        phoneNumber.remove(phoneNumber.length() - 1);
                        updateDisplayWhileInputingNumber();
                    }
                }

                // SUPPRIMER TOUTE LA COMPOSITION ACTUELLE
                if (key == 'C')
                {
                    phoneNumber = "";
                    updateDisplayWhileInputingNumber();
                }
            }
            // SI UN APPEL EN COURS
            // ACTION POSSIBLES SI APPEL EN COURS
            else
            {
                // RACCROCHER SI D
                if (key == 'D')
                {
                    outgoingCall = false;
                    incomingCall = false;
                    ongoingCall = false;
                    incomingCallNumber = "";
                    hangUp();
                    updateDisplayWhileCalling(true);
                    delay(1000);
                    updateDisplayWhileInputingNumber();
                }
            }

            // APPUI SUR A
            if (key == 'A')
            {
                // DECROCHER SI APPEL RENTRANT ET PAS D'APPEL EN COURS
                if (incomingCall && !ongoingCall)
                {
                    ongoingCall = true;
                    outgoingCall = false;
                    receiveCall();
                    updateDisplayWhileCalling();
                }
                else
                {
                    // REMETTRE L'ANCIEN NUMERO
                    if (phoneNumber.isEmpty())
                    {
                        phoneNumber = oldPhoneNumber;
                        updateDisplayWhileInputingNumber();
                    }
                    // APPELER
                    else
                    {
                        outgoingCall = true;
                        makeCall();
                        updateDisplayWhileCalling();
                        oldPhoneNumber = phoneNumber;
                        phoneNumber = "";
                    }
                }
            }
        }
    }
}

// AFFICHAGE SUR L'ECRAN SI ON EST ENTRAIN DE SAISIR UN NUMERO
void updateDisplayWhileInputingNumber()
{
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println("Entrer le numero: ");
    display.setCursor(0, 30);
    display.println(phoneNumber);
    display.display();
}

// AFFICHAGE SUR L'ECRAN LORS DES APPELS
void updateDisplayWhileCalling(bool d)

{
    display.clearDisplay();
    display.setCursor(0, 10);
    // SI UN APPEL EST EN COURS (DECROCHE)
    if (ongoingCall)
    {
        display.println("Appel en cours");
        display.setCursor(0, 30);
        display.println(incomingCall ? incomingCallNumber : phoneNumber);
    }
    // SI UN APPEL SORTANT EST EFFECTUE (LANCE)
    else if (outgoingCall)
    {
        display.println("Appel sortant");
        display.setCursor(0, 30);
        display.println(phoneNumber);
    }
    // SI UN APPEL ENTRANT EST DECOUVERT (ENTRANT MAIS PAS DECROCHE)
    else if (incomingCall)
    {
        display.println("Appel entrant");
        display.setCursor(0, 30);
        display.println(incomingCallNumber);
    }
    // SI ON ARRIVE LA, C'EST QU'ON A RACCROCHE
    else if (d)
    {
        display.println("Fin de l'appel");
    }
    else
        ;

    display.display();
}

// POUR DECROCHER
void receiveCall()
{
    updateDisplayWhileCalling();
    SIM800.println("ATA");
    delay(1000);
}

// POUR APPELER
void makeCall()
{
    updateDisplayWhileCalling();
    SIM800.print("ATD");
    SIM800.print(phoneNumber);
    SIM800.println(";");
    delay(1000);
}

// POUR RACCROCHER
void hangUp()
{
    SIM800.println("ATH");
    delay(1000);
    updateDisplayWhileCalling();
}

// INITIALISER LE MODULE SIM800L CONFIGURE EN EXTERNE
void setupSim()
{
    SIM800.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX); // Initialize Serial1 for SIM800L
    delay(1000);

    SIM800.println("AT");

    delay(1000);

    pinMode(RING_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(RING_PIN), ringISR, FALLING);

    if (SIM800.available())
    {
        while (SIM800.available())
        {
            Serial.write(SIM800.read());
        }
    }
    else
    {
        Serial.println("Aucune réponse de la SIM");
    }
}

// INITIALISER L'ECRAN
void setupScreen()
{
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("Ecran inacessible"));
        for (;;)
            ;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.println("Entrer le numero: ");
    display.display();
}

// DEMANDE DU NUMERO ACTUEL
void getCurrentCallingNumber()
{
    SIM800.println("AT+CLCC");
    delay(1000);

    if (SIM800.available())
    {
        String response = "";
        while (SIM800.available())
        {
            response += (char)SIM800.read();
        }
        Serial.println(response);
        parseCLCCResponse(response);
    }
}

// RECUPERER LE NUMERO SEULEMENT DE TOUTE LA REPONSE ENVOYEE
void parseCLCCResponse(String response)
{
    int index = response.indexOf("\"");
    if (index != -1)
    {
        int endIndex = response.indexOf("\"", index + 1);
        if (endIndex != -1)
        {
            String callingNumber = response.substring(index + 1, endIndex);
            incomingCallNumber = callingNumber;
        }
    }
    else
    {
        Serial.println("Pas d'appel entrant détecté");
    }
}

void IRAM_ATTR ringISR()
{
    incomingCall = true;
    updateDisplayWhileCalling();
}