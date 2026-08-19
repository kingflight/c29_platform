#include "shell.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "motor_control.h"
#include "motor_state.h"
#include "uart.h"

#define SHELL_HISTORY_DEPTH 10U
#define SHELL_LINE_BUFFER_SIZE 64U
#define SHELL_PROMPT "> "

static char shellLineBuffer[SHELL_LINE_BUFFER_SIZE];
static size_t shellLineLength = 0U;
static char shellHistory[SHELL_HISTORY_DEPTH][SHELL_LINE_BUFFER_SIZE];
static size_t shellHistoryCount = 0U;
static size_t shellHistoryNextIndex = 0U;
static int shellHistoryBrowseOffset = -1;
static char shellDraftBuffer[SHELL_LINE_BUFFER_SIZE];

typedef enum
{
    SHELL_ESCAPE_STATE_IDLE = 0,
    SHELL_ESCAPE_STATE_ESC = 1,
    SHELL_ESCAPE_STATE_CSI = 2
} ShellEscapeState;

static ShellEscapeState shellEscapeState = SHELL_ESCAPE_STATE_IDLE;

static bool isSpaceChar(char c)
{
    return (c == ' ') || (c == '\t');
}

static bool stringsEqual(const char *left, const char *right)
{
    while((*left != '\0') && (*right != '\0'))
    {
        if(*left != *right)
        {
            return false;
        }

        left++;
        right++;
    }

    return (*left == '\0') && (*right == '\0');
}

static char *skipSpaces(char *text)
{
    while(isSpaceChar(*text))
    {
        text++;
    }

    return text;
}

static bool parseFloatArgument(char *text, float *value)
{
    char *endPtr;

    text = skipSpaces(text);

    if(*text == '\0')
    {
        return false;
    }

    *value = strtof(text, &endPtr);

    if(endPtr == text)
    {
        return false;
    }

    endPtr = skipSpaces(endPtr);

    return *endPtr == '\0';
}

static bool parseTwoFloatArguments(char *text, float *first, float *second)
{
    char *endPtr;

    text = skipSpaces(text);

    if(*text == '\0')
    {
        return false;
    }

    *first = strtof(text, &endPtr);

    if(endPtr == text)
    {
        return false;
    }

    text = skipSpaces(endPtr);
    *second = strtof(text, &endPtr);

    if(endPtr == text)
    {
        return false;
    }

    endPtr = skipSpaces(endPtr);

    return *endPtr == '\0';
}

static bool parseThreeFloatArguments(char *text, float *first, float *second,
                                     float *third)
{
    char *endPtr;

    text = skipSpaces(text);

    if(*text == '\0')
    {
        return false;
    }

    *first = strtof(text, &endPtr);
    if(endPtr == text)
    {
        return false;
    }

    text = skipSpaces(endPtr);
    *second = strtof(text, &endPtr);
    if(endPtr == text)
    {
        return false;
    }

    text = skipSpaces(endPtr);
    *third = strtof(text, &endPtr);
    if(endPtr == text)
    {
        return false;
    }

    endPtr = skipSpaces(endPtr);
    return *endPtr == '\0';
}

static void printPrompt(void)
{
    Uart_writeString(SHELL_PROMPT);
}

static void writeChar(char c)
{
    char text[2];

    text[0] = c;
    text[1] = '\0';
    Uart_writeString(text);
}

static void redrawLine(void)
{
    Uart_writeString("\r\x1b[2K");
    printPrompt();

    if(shellLineLength > 0U)
    {
        shellLineBuffer[shellLineLength] = '\0';
        Uart_writeString(shellLineBuffer);
    }
}

static void saveHistory(const char *line)
{
    size_t i;

    if(*line == '\0')
    {
        return;
    }

    for(i = 0U; i < shellHistoryCount; i++)
    {
        size_t historyIndex =
            (shellHistoryNextIndex + SHELL_HISTORY_DEPTH - 1U - i) %
            SHELL_HISTORY_DEPTH;

        if(strcmp(shellHistory[historyIndex], line) == 0)
        {
            return;
        }
    }

    strncpy(shellHistory[shellHistoryNextIndex], line, SHELL_LINE_BUFFER_SIZE - 1U);
    shellHistory[shellHistoryNextIndex][SHELL_LINE_BUFFER_SIZE - 1U] = '\0';
    shellHistoryNextIndex = (shellHistoryNextIndex + 1U) % SHELL_HISTORY_DEPTH;

    if(shellHistoryCount < SHELL_HISTORY_DEPTH)
    {
        shellHistoryCount++;
    }
}

static void loadHistoryEntry(size_t offsetFromNewest)
{
    size_t historyIndex;

    historyIndex =
        (shellHistoryNextIndex + SHELL_HISTORY_DEPTH - 1U - offsetFromNewest) %
        SHELL_HISTORY_DEPTH;

    strncpy(shellLineBuffer, shellHistory[historyIndex], SHELL_LINE_BUFFER_SIZE - 1U);
    shellLineBuffer[SHELL_LINE_BUFFER_SIZE - 1U] = '\0';
    shellLineLength = strlen(shellLineBuffer);
}

static void browseHistoryOlder(void)
{
    if(shellHistoryCount == 0U)
    {
        return;
    }

    if(shellHistoryBrowseOffset < 0)
    {
        strncpy(shellDraftBuffer, shellLineBuffer, SHELL_LINE_BUFFER_SIZE - 1U);
        shellDraftBuffer[SHELL_LINE_BUFFER_SIZE - 1U] = '\0';
        shellHistoryBrowseOffset = 0;
    }
    else if((size_t)(shellHistoryBrowseOffset + 1) < shellHistoryCount)
    {
        shellHistoryBrowseOffset++;
    }

    loadHistoryEntry((size_t)shellHistoryBrowseOffset);
    redrawLine();
}

static void browseHistoryNewer(void)
{
    if(shellHistoryBrowseOffset < 0)
    {
        return;
    }

    shellHistoryBrowseOffset--;

    if(shellHistoryBrowseOffset < 0)
    {
        strncpy(shellLineBuffer, shellDraftBuffer, SHELL_LINE_BUFFER_SIZE - 1U);
        shellLineBuffer[SHELL_LINE_BUFFER_SIZE - 1U] = '\0';
        shellLineLength = strlen(shellLineBuffer);
    }
    else
    {
        loadHistoryEntry((size_t)shellHistoryBrowseOffset);
    }

    redrawLine();
}

static void printHelp(void)
{
    Uart_writeString(
        "Commands: help, status, stop, id <A>, iq <A>, refs <id_A> <iq_A>, duty <a> <b> <c>\r\n");
}

static void printStatus(void)
{
    Uart_printf("STATE=%s ID_REF=%.3f IQ_REF=%.3f READY=%u\r\n",
                MotorControl_getStateName(),
                g_motorControlState.current_d_ref,
                g_motorControlState.current_q_ref,
                MotorControl_isReady() ? 1U : 0U);
}

static void handleLine(char *line)
{
    char *command;
    char *arguments;
    float valueA;
    float valueB;
    float valueC;

    command = skipSpaces(line);

    if(*command == '\0')
    {
        return;
    }

    arguments = command;

    while((*arguments != '\0') && !isSpaceChar(*arguments))
    {
        arguments++;
    }

    if(*arguments != '\0')
    {
        *arguments = '\0';
        arguments++;
    }

    if(stringsEqual(command, "help"))
    {
        printHelp();
        return;
    }

    if(stringsEqual(command, "status"))
    {
        printStatus();
        return;
    }

    if(stringsEqual(command, "stop"))
    {
        MotorControl_stop();
        Uart_writeString("OK stop\r\n");
        return;
    }

    if(stringsEqual(command, "id"))
    {
        if(!parseFloatArgument(arguments, &valueA))
        {
            Uart_writeString("ERR usage: id <amps>\r\n");
            return;
        }

        MotorControl_setCurrentReference(valueA, g_motorControlState.current_q_ref);
        Uart_printf("OK ID_REF=%.3f IQ_REF=%.3f\r\n",
                    g_motorControlState.current_d_ref,
                    g_motorControlState.current_q_ref);
        return;
    }

    if(stringsEqual(command, "iq"))
    {
        if(!parseFloatArgument(arguments, &valueA))
        {
            Uart_writeString("ERR usage: iq <amps>\r\n");
            return;
        }

        MotorControl_setCurrentReference(g_motorControlState.current_d_ref, valueA);
        Uart_printf("OK ID_REF=%.3f IQ_REF=%.3f\r\n",
                    g_motorControlState.current_d_ref,
                    g_motorControlState.current_q_ref);
        return;
    }

    if(stringsEqual(command, "refs"))
    {
        if(!parseTwoFloatArguments(arguments, &valueA, &valueB))
        {
            Uart_writeString("ERR usage: refs <id_amps> <iq_amps>\r\n");
            return;
        }

        MotorControl_setCurrentReference(valueA, valueB);
        Uart_printf("OK ID_REF=%.3f IQ_REF=%.3f\r\n",
                    g_motorControlState.current_d_ref,
                    g_motorControlState.current_q_ref);
        return;
    }

    if(stringsEqual(command, "duty"))
    {
        if(!parseThreeFloatArguments(arguments, &valueA, &valueB, &valueC))
        {
            Uart_writeString("ERR usage: duty <phaseA_0to1> <phaseB_0to1> <phaseC_0to1>\r\n");
            return;
        }

        MotorControl_setPhaseDutyCycles(valueA, valueB, valueC);
        Uart_printf("OK DUTY_A=%.3f DUTY_B=%.3f DUTY_C=%.3f\r\n",
                    g_motorControlState.pwm_duty_a,
                    g_motorControlState.pwm_duty_b,
                    g_motorControlState.pwm_duty_c);
        return;
    }

    Uart_writeString("ERR unknown command\r\n");
}

void Shell_init(void)
{
    shellLineLength = 0U;
    shellHistoryBrowseOffset = -1;
    shellEscapeState = SHELL_ESCAPE_STATE_IDLE;
    Uart_writeString("Shell ready. Type 'help'.\r\n");
    printPrompt();
}

void Shell_process(void)
{
    int32_t receivedChar;

    Uart_clearRxErrors();

    while((receivedChar = Uart_readCharNonBlocking()) >= 0)
    {
        char c = (char)receivedChar;

        if(shellEscapeState == SHELL_ESCAPE_STATE_ESC)
        {
            shellEscapeState = (c == '[') ? SHELL_ESCAPE_STATE_CSI :
                                            SHELL_ESCAPE_STATE_IDLE;
            continue;
        }

        if(shellEscapeState == SHELL_ESCAPE_STATE_CSI)
        {
            shellEscapeState = SHELL_ESCAPE_STATE_IDLE;

            if(c == 'A')
            {
                browseHistoryOlder();
            }
            else if(c == 'B')
            {
                browseHistoryNewer();
            }

            continue;
        }

        if(c == 0x1b)
        {
            shellEscapeState = SHELL_ESCAPE_STATE_ESC;
            continue;
        }

        if((c == '\r') || (c == '\n'))
        {
            if(shellLineLength > 0U)
            {
                shellLineBuffer[shellLineLength] = '\0';
                saveHistory(shellLineBuffer);
                shellHistoryBrowseOffset = -1;
                shellDraftBuffer[0] = '\0';
                Uart_writeString("\r\n");
                handleLine(shellLineBuffer);
                shellLineLength = 0U;
            }
            else
            {
                Uart_writeString("\r\n");
            }

            printPrompt();

            continue;
        }

        if((c == '\b') || (c == 0x7f))
        {
            if(shellLineLength > 0U)
            {
                shellLineLength--;
                shellLineBuffer[shellLineLength] = '\0';
                redrawLine();
            }

            continue;
        }

        if(shellLineLength < (SHELL_LINE_BUFFER_SIZE - 1U))
        {
            shellLineBuffer[shellLineLength] = c;
            shellLineLength++;
            shellLineBuffer[shellLineLength] = '\0';
            shellHistoryBrowseOffset = -1;
            writeChar(c);
        }
        else
        {
            shellLineLength = 0U;
            shellHistoryBrowseOffset = -1;
            Uart_writeString("\r\nERR command too long\r\n");
            printPrompt();
        }
    }
}

int Shell_isEditingLine(void)
{
    return shellLineLength > 0U;
}
