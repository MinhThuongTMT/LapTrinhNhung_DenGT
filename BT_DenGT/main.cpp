#include "mbed.h"
#include "arm_book_lib.h"
#include <stdlib.h> // Để sử dụng atoi()
#include <string.h> // Để sử dụng memset() và strlen()

//=====[Defines]===============================================================

#define NUMBER_OF_KEYS                           4
#define BLINKING_TIME_GAS_ALARM               1000
#define BLINKING_TIME_OVER_TEMP_ALARM          500
#define BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM  100
#define NUMBER_OF_AVG_SAMPLES                   100
#define OVER_TEMP_LEVEL                         50
#define TIME_INCREMENT_MS                       10
#define DEBOUNCE_BUTTON_TIME_MS                 40
#define KEYPAD_NUMBER_OF_ROWS                    4
#define KEYPAD_NUMBER_OF_COLS                    4

//=====[Declaration of public data types]======================================

typedef enum {
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_FALLING,
    BUTTON_RISING
} buttonState_t;

typedef enum {
    MATRIX_KEYPAD_SCANNING,
    MATRIX_KEYPAD_DEBOUNCE,
    MATRIX_KEYPAD_KEY_HOLD_PRESSED
} matrixKeypadState_t;

//=====[Declaration and initialization of public global objects]===============

DigitalIn enterButton(BUTTON1);
DigitalIn alarmTestButton(D2);
DigitalIn aButton(D4);
DigitalIn bButton(D5);
DigitalIn cButton(D6);
DigitalIn dButton(D7);
DigitalIn mq2(PE_12);

DigitalOut alarmLed(LED1);        // Đèn Đỏ
DigitalOut incorrectCodeLed(LED3); // Đèn Vàng
DigitalOut systemBlockedLed(LED2); // Đèn Xanh

DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn lm35(A1);

DigitalOut keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn keypadColPins[KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

//=====[Declaration and initialization of public global variables]=============

bool alarmState    = OFF;
bool incorrectCode = false;
bool overTempDetector = OFF;

int numberOfIncorrectCodes = 0;
int buttonBeingCompared    = 0;
int codeSequence[NUMBER_OF_KEYS]   = { 1, 1, 0, 0 };
int buttonsPressed[NUMBER_OF_KEYS] = { 0, 0, 0, 0 };
int accumulatedTimeAlarm = 0;

bool gasDetectorState          = OFF;
bool overTempDetectorState     = OFF;

float potentiometerReading = 0.0;
float lm35ReadingsAverage  = 0.0;
float lm35ReadingsSum      = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC            = 0.0;

int accumulatedDebounceButtonTime     = 0;
int numberOfEnterButtonReleasedEvents = 0;
buttonState_t enterButtonState;

int accumulatedDebounceMatrixKeypadTime = 0;
char matrixKeypadLastKeyPressed = '\0';
char matrixKeypadIndexToCharArray[] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D',
};
matrixKeypadState_t matrixKeypadState;

// Biến thời gian sáng của các đèn
int greenOnTime = 200;    // Thời gian sáng của đèn Xanh (ms)
int redBlinkTime = 50;    // Thời gian sáng của đèn Đỏ (ms)
int yellowOnTime = 100;   // Thời gian sáng của đèn Vàng (ms)

// Biến để xử lý chế độ và nhập mật khẩu
static bool autoMode = true;       // Bắt đầu ở chế độ tự động
static bool manualMode = false;    // Chế độ thủ công
static bool settingTimes = false;  // Trạng thái chế độ cài đặt thời gian
static char inputBuffer[10] = {0}; // Bộ đệm lưu số nhập từ bàn phím
static int inputIndex = 0;         // Chỉ số của bộ đệm
static char selectedLed = '\0';    // Biến để theo dõi đèn được chọn trong chế độ cài đặt

// Mật khẩu
const char passwordX[] = "7373"; // Mật khẩu để chuyển sang thủ công
const char passwordY[] = "8888"; // Mật khẩu để quay về tự động
const char passwordZ[] = "0000"; // Mật khẩu để thay đổi thời gian

// Biến để xử lý nhập mật khẩu
static bool enteringPassword = false; // Trạng thái nhập mật khẩu
static char enteredPassword[10] = {0}; // Bộ đệm lưu mật khẩu nhập vào
static int passwordIndex = 0;         // Chỉ số của mật khẩu nhập vào

// Biến cho state machine ở chế độ tự động
static int autoState = 0; // 0: green, 1: red, 2: yellow
static int stateTime = 0; // Thời gian trong trạng thái hiện tại

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();
void alarmActivationUpdate();
void alarmDeactivationUpdate();
void uartTask();
void availableCommands();
bool areEqual();
float celsiusToFahrenheit(float tempInCelsiusDegrees);
float analogReadingScaledWithTheLM35Formula(float analogReading);
void lm35ReadingsArrayInit();
void debounceButtonInit();
bool debounceButtonUpdate();
void matrixKeypadInit();
char matrixKeypadScan();
char matrixKeypadUpdate();

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    inputsInit();
    outputsInit();
    while (true) {
        alarmActivationUpdate();
        alarmDeactivationUpdate();
        uartTask();
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    lm35ReadingsArrayInit();
    alarmTestButton.mode(PullDown);
    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    dButton.mode(PullDown);
    sirenPin.mode(OpenDrain);
    sirenPin.input();
    debounceButtonInit();
    matrixKeypadInit();
}

void outputsInit()
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

void alarmActivationUpdate()
{
    static int lm35SampleIndex = 0;
    int i = 0;

    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;
    if (lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        lm35SampleIndex = 0;
    }
    
    lm35ReadingsSum = 0.0;
    for (i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsSum += lm35ReadingsArray[i];
    }
    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    lm35TempC = analogReadingScaledWithTheLM35Formula(lm35ReadingsAverage);    
    
    if (lm35TempC > OVER_TEMP_LEVEL) {
        overTempDetector = ON;
    } else {
        overTempDetector = OFF;
    }

    if (!mq2) {                      
        gasDetectorState = ON;
    }
    if (overTempDetector) {
        overTempDetectorState = ON;
    }
    if (alarmTestButton) {             
        overTempDetectorState = ON;
        gasDetectorState = ON;
    }    
    if (alarmState) { 
        accumulatedTimeAlarm += TIME_INCREMENT_MS;
        sirenPin.output();                                     
        sirenPin = LOW;                                        
    
        if (gasDetectorState && overTempDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if (gasDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_GAS_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if (overTempDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_OVER_TEMP_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        }
    } else {
        alarmLed = OFF;
        gasDetectorState = OFF;
        overTempDetectorState = OFF;
        sirenPin.input();                                  
    }
}

void alarmDeactivationUpdate()
{
    char keyReleased = matrixKeypadUpdate();

    // Xử lý khi nhấn '*': tắt tất cả LED và bắt đầu nhập mật khẩu
    if (keyReleased == '*' && !enteringPassword && !settingTimes) {
        enteringPassword = true;
        passwordIndex = 0;
        memset(enteredPassword, 0, sizeof(enteredPassword));
        systemBlockedLed = OFF;
        alarmLed = OFF;
        incorrectCodeLed = OFF;
        uartUsb.write("Nhap mat khau (ket thuc bang '#')\r\n", 37);
    }

    // Xử lý nhập mật khẩu
    if (enteringPassword) {
        if (keyReleased != '\0' && keyReleased != '*') {
            if (keyReleased == '#') {
                enteredPassword[passwordIndex] = '\0';
                if (strcmp(enteredPassword, passwordX) == 0) {
                    uartUsb.write("Mat khau X dung. Chuyen sang che do thu cong.\r\n", 47);
                    manualMode = true;
                    autoMode = false;
                    systemBlockedLed = OFF;
                    alarmLed = OFF;
                    incorrectCodeLed = OFF;
                } else if (strcmp(enteredPassword, passwordY) == 0) {
                    uartUsb.write("Mat khau Y dung. Quay ve che do tu dong.\r\n", 42);
                    autoMode = true;
                    manualMode = false;
                    autoState = 0;
                    stateTime = 0;
                    systemBlockedLed = OFF;
                    alarmLed = OFF;
                    incorrectCodeLed = OFF;
                } else if (strcmp(enteredPassword, passwordZ) == 0 && autoMode) {
                    uartUsb.write("Mat khau Z dung. Bat dau thay doi thoi gian.\r\n", 44);
                    settingTimes = true;
                    selectedLed = '\0';
                    inputIndex = 0;
                    memset(inputBuffer, 0, sizeof(inputBuffer));
                    uartUsb.write("Chon den de cai dat: 'B' cho Blue, 'C' cho Yellow, 'D' cho Red, '*' de thoat\r\n", 77);
                } else {
                    uartUsb.write("Mat khau sai hoac che do khong hop le.\r\n", 41);
                }
                enteringPassword = false;
            } else if (passwordIndex < 9) {
                enteredPassword[passwordIndex] = keyReleased;
                passwordIndex++;
                uartUsb.write(&keyReleased, 1);
            }
        }
    }
    // Xử lý cài đặt thời gian
    else if (settingTimes) {
        if (selectedLed == '\0') {
            // Chọn đèn
            if (keyReleased == 'B' || keyReleased == 'C' || keyReleased == 'D') {
                selectedLed = keyReleased;
                uartUsb.write("Nhap thoi gian moi (ms), sau do nhan 'A'\r\n", 44);
            } else if (keyReleased == '*') {
                settingTimes = false;
                uartUsb.write("Cap nhat thoi gian thanh cong.\r\n", 33);
            }
        } else {
            // Nhập thời gian cho đèn đã chọn
            if (keyReleased >= '0' && keyReleased <= '9') {
                if (inputIndex < 9) {
                    inputBuffer[inputIndex] = keyReleased;
                    inputIndex++;
                }
            } else if (keyReleased == 'A') {
                inputBuffer[inputIndex] = '\0';
                int duration = atoi(inputBuffer);
                if (selectedLed == 'B') {
                    greenOnTime = duration;
                    uartUsb.write("Thoi gian sang den X dat thanh ", 37);
                } else if (selectedLed == 'C') {
                    redBlinkTime = duration;
                    uartUsb.write("Thoi gian sang den V dat thanh ", 38);
                } else if (selectedLed == 'D') {
                    yellowOnTime = duration;
                    uartUsb.write("Thoi gian sang den D dat thanh ",39);
                }
                char str[10];
                sprintf(str, "%d ms\r\n", duration);
                uartUsb.write(str, strlen(str));
                selectedLed = '\0'; // Quay lại trạng thái chọn đèn
                inputIndex = 0;
                memset(inputBuffer, 0, sizeof(inputBuffer));
                uartUsb.write("Chon den khac hoac nhan '*' de cap nhat\r\n", 39);
            }
        }
    }
    // Xử lý chế độ tự động và thủ công
    else {
        if (autoMode && !alarmState) {
            if (autoState == 0) {
                systemBlockedLed = ON;
                alarmLed = OFF;
                incorrectCodeLed = OFF;
                if (stateTime >= greenOnTime) {
                    autoState = 1;
                    stateTime = 0;
                }
            } else if (autoState == 1) {
                systemBlockedLed = OFF;
                alarmLed = ON;
                incorrectCodeLed = OFF;
                if (stateTime >= redBlinkTime) {
                    autoState = 2;
                    stateTime = 0;
                }
            } else if (autoState == 2) {
                systemBlockedLed = OFF;
                alarmLed = OFF;
                incorrectCodeLed = ON;
                if (stateTime >= yellowOnTime) {
                    autoState = 0;
                    stateTime = 0;
                }
            }
            stateTime += TIME_INCREMENT_MS;
        } else if (manualMode) {
            if (alarmTestButton) { // Nhấn D2 → Đèn Đỏ nhấp nháy 1s
                alarmLed = ON;
                incorrectCodeLed = OFF;
                systemBlockedLed = OFF;
                delay(1000);
                alarmLed = OFF;
                delay(1000);
            }
            if (aButton) { // Nhấn D4 → Đèn Vàng nhấp nháy 1s
                incorrectCodeLed = ON;
                alarmLed = OFF;
                systemBlockedLed = OFF;
                delay(1000);
                incorrectCodeLed = OFF;
                delay(1000);
            }
            if (bButton) { // Nhấn D5 → Đèn Xanh nhấp nháy 1s
                systemBlockedLed = ON;
                incorrectCodeLed = OFF;
                alarmLed = OFF;
                delay(1000);
                systemBlockedLed = OFF;
                delay(1000);
            }
        }
    }
}

void uartTask()
{
    char receivedChar = '\0';
    char str[100];
    int stringLength;
    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
        switch (receivedChar) {
        case '1':
            if (alarmState) {
                uartUsb.write("The alarm is activated\r\n", 24);
            } else {
                uartUsb.write("The alarm is not activated\r\n", 28);
            }
            break;

        case '2':
            if (!mq2) {
                uartUsb.write("Gas is being detected\r\n", 22);
            } else {
                uartUsb.write("Gas is not being detected\r\n", 27);
            }
            break;

        case '3':
            if (overTempDetector) {
                uartUsb.write("Temperature is above the maximum level\r\n", 40);
            } else {
                uartUsb.write("Temperature is below the maximum level\r\n", 40);
            }
            break;
            
        case '4':
            uartUsb.write("Please enter the code sequence.\r\n", 33);
            uartUsb.write("First enter 'A', then 'B', then 'C', and ", 41);
            uartUsb.write("finally 'D' button\r\n", 20);
            uartUsb.write("In each case type 1 for pressed or 0 for ", 41);
            uartUsb.write("not pressed\r\n", 13);
            uartUsb.write("For example, for 'A' = pressed, ", 32);
            uartUsb.write("'B' = pressed, 'C' = not pressed, ", 34);
            uartUsb.write("'D' = not pressed, enter '1', then '1', ", 40);
            uartUsb.write("then '0', and finally '0'\r\n\r\n", 29);

            incorrectCode = false;

            for (buttonBeingCompared = 0; buttonBeingCompared < NUMBER_OF_KEYS; buttonBeingCompared++) {
                uartUsb.read(&receivedChar, 1);
                uartUsb.write("*", 1);

                if (receivedChar == '1') {
                    if (codeSequence[buttonBeingCompared] != 1) {
                        incorrectCode = true;
                    }
                } else if (receivedChar == '0') {
                    if (codeSequence[buttonBeingCompared] != 0) {
                        incorrectCode = true;
                    }
                } else {
                    incorrectCode = true;
                }
            }

            if (!incorrectCode) {
                uartUsb.write("\r\nThe code is correct\r\n\r\n", 25);
                alarmState = OFF;
                incorrectCodeLed = OFF;
                numberOfIncorrectCodes = 0;
            } else {
                uartUsb.write("\r\nThe code is incorrect\r\n\r\n", 27);
                incorrectCodeLed = ON;
                numberOfIncorrectCodes++;
            }
            break;

        case '5':
            uartUsb.write("Please enter new code sequence\r\n", 32);
            uartUsb.write("First enter 'A', then 'B', then 'C', and ", 41);
            uartUsb.write("finally 'D' button\r\n", 20);
            uartUsb.write("In each case type 1 for pressed or 0 for not ", 45);
            uartUsb.write("pressed\r\n", 9);
            uartUsb.write("For example, for 'A' = pressed, 'B' = pressed,", 46);
            uartUsb.write(" 'C' = not pressed,", 19);
            uartUsb.write("'D' = not pressed, enter '1', then '1', ", 40);
            uartUsb.write("then '0', and finally '0'\r\n\r\n", 29);

            for (buttonBeingCompared = 0; buttonBeingCompared < NUMBER_OF_KEYS; buttonBeingCompared++) {
                uartUsb.read(&receivedChar, 1);
                uartUsb.write("*", 1);

                if (receivedChar == '1') {
                    codeSequence[buttonBeingCompared] = 1;
                } else if (receivedChar == '0') {
                    codeSequence[buttonBeingCompared] = 0;
                }
            }

            uartUsb.write("\r\nNew code generated\r\n\r\n", 24);
            break;

        case 'c':
        case 'C':
            sprintf(str, "Temperature: %.2f \xB0 C\r\n", lm35TempC);
            stringLength = strlen(str);
            uartUsb.write(str, stringLength);
            break;

        case 'f':
        case 'F':
            sprintf(str, "Temperature: %.2f \xB0 F\r\n", celsiusToFahrenheit(lm35TempC));
            stringLength = strlen(str);
            uartUsb.write(str, stringLength);
            break;

        default:
            availableCommands();
            break;
        }
    }
}

void availableCommands()
{
    uartUsb.write("Available commands:\r\n", 21);
    uartUsb.write("Press '1' to get the alarm state\r\n", 34);
    uartUsb.write("Press '2' to get the gas detector state\r\n", 41);
    uartUsb.write("Press '3' to get the over temperature detector state\r\n", 54);
    uartUsb.write("Press '4' to enter the code sequence\r\n", 38);
    uartUsb.write("Press '5' to enter a new code\r\n", 31);
    uartUsb.write("Press 'f' or 'F' to get lm35 reading in Fahrenheit\r\n", 52);
    uartUsb.write("Press 'c' or 'C' to get lm35 reading in Celsius\r\n\r\n", 51);
}

bool areEqual()
{
    int i;
    for (i = 0; i < NUMBER_OF_KEYS; i++) {
        if (codeSequence[i] != buttonsPressed[i]) {
            return false;
        }
    }
    return true;
}

float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    return (analogReading * 3.3 / 0.01);
}

float celsiusToFahrenheit(float tempInCelsiusDegrees)
{
    return (tempInCelsiusDegrees * 9.0 / 5.0 + 32.0);
}

void lm35ReadingsArrayInit()
{
    int i;
    for (i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsArray[i] = 0;
    }
}

void debounceButtonInit()
{
    if (enterButton) {
        enterButtonState = BUTTON_DOWN;
    } else {
        enterButtonState = BUTTON_UP;
    }
}

bool debounceButtonUpdate()
{
    bool enterButtonReleasedEvent = false;
    switch (enterButtonState) {
    case BUTTON_UP:
        if (enterButton) {
            enterButtonState = BUTTON_FALLING;
            accumulatedDebounceButtonTime = 0;
        }
        break;

    case BUTTON_FALLING:
        if (accumulatedDebounceButtonTime >= DEBOUNCE_BUTTON_TIME_MS) {
            if (enterButton) {
                enterButtonState = BUTTON_DOWN;
            } else {
                enterButtonState = BUTTON_UP;
            }
        }
        accumulatedDebounceButtonTime += TIME_INCREMENT_MS;
        break;

    case BUTTON_DOWN:
        if (!enterButton) {
            enterButtonState = BUTTON_RISING;
            accumulatedDebounceButtonTime = 0;
        }
        break;

    case BUTTON_RISING:
        if (accumulatedDebounceButtonTime >= DEBOUNCE_BUTTON_TIME_MS) {
            if (!enterButton) {
                enterButtonState = BUTTON_UP;
                enterButtonReleasedEvent = true;
            } else {
                enterButtonState = BUTTON_DOWN;
            }
        }
        accumulatedDebounceButtonTime += TIME_INCREMENT_MS;
        break;

    default:
        debounceButtonInit();
        break;
    }
    return enterButtonReleasedEvent;
}

void matrixKeypadInit()
{
    matrixKeypadState = MATRIX_KEYPAD_SCANNING;
    int pinIndex = 0;
    for (pinIndex = 0; pinIndex < KEYPAD_NUMBER_OF_COLS; pinIndex++) {
        keypadColPins[pinIndex].mode(PullUp);
    }
}

char matrixKeypadScan()
{
    int row = 0;
    int col = 0;
    int i = 0;

    for (row = 0; row < KEYPAD_NUMBER_OF_ROWS; row++) {
        for (i = 0; i < KEYPAD_NUMBER_OF_ROWS; i++) {
            keypadRowPins[i] = ON;
        }
        keypadRowPins[row] = OFF;

        for (col = 0; col < KEYPAD_NUMBER_OF_COLS; col++) {
            if (keypadColPins[col] == OFF) {
                return matrixKeypadIndexToCharArray[row * KEYPAD_NUMBER_OF_ROWS + col];
            }
        }
    }
    return '\0';
}

char matrixKeypadUpdate()
{
    char keyDetected = '\0';
    char keyReleased = '\0';

    switch (matrixKeypadState) {
    case MATRIX_KEYPAD_SCANNING:
        keyDetected = matrixKeypadScan();
        if (keyDetected != '\0') {
            matrixKeypadLastKeyPressed = keyDetected;
            accumulatedDebounceMatrixKeypadTime = 0;
            matrixKeypadState = MATRIX_KEYPAD_DEBOUNCE;
        }
        break;

    case MATRIX_KEYPAD_DEBOUNCE:
        if (accumulatedDebounceMatrixKeypadTime >= DEBOUNCE_BUTTON_TIME_MS) {
            keyDetected = matrixKeypadScan();
            if (keyDetected == matrixKeypadLastKeyPressed) {
                matrixKeypadState = MATRIX_KEYPAD_KEY_HOLD_PRESSED;
            } else {
                matrixKeypadState = MATRIX_KEYPAD_SCANNING;
            }
        }
        accumulatedDebounceMatrixKeypadTime += TIME_INCREMENT_MS;
        break;

    case MATRIX_KEYPAD_KEY_HOLD_PRESSED:
        keyDetected = matrixKeypadScan();
        if (keyDetected != matrixKeypadLastKeyPressed) {
            if (keyDetected == '\0') {
                keyReleased = matrixKeypadLastKeyPressed;
            }
            matrixKeypadState = MATRIX_KEYPAD_SCANNING;
        }
        break;

    default:
        matrixKeypadInit();
        break;
    }
    return keyReleased;
}