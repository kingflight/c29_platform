#include "encoder.h"

#include "driverlib.h"

#include "motor_state.h"

#define ENCODER_QEPA_GPIO          11U
#define ENCODER_QEPB_GPIO          10U
#define ENCODER_QEPI_GPIO          32U
#define ENCODER_MAX_POSITION       0xFFFFFFFFUL
#define ENCODER_UNIT_TIMER_PERIOD  2000000UL

static void configureEncoderPins(void)
{
    GPIO_setPinConfig(GPIO_11_GPIO11);
    GPIO_setPinConfig(GPIO_10_GPIO10);
    GPIO_setPinConfig(GPIO_32_GPIO32);

    GPIO_setDirectionMode(ENCODER_QEPA_GPIO, GPIO_DIR_MODE_IN);
    GPIO_setDirectionMode(ENCODER_QEPB_GPIO, GPIO_DIR_MODE_IN);
    GPIO_setDirectionMode(ENCODER_QEPI_GPIO, GPIO_DIR_MODE_IN);

    GPIO_setPadConfig(ENCODER_QEPA_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPadConfig(ENCODER_QEPB_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_setPadConfig(ENCODER_QEPI_GPIO, GPIO_PIN_TYPE_PULLUP);

    GPIO_setQualificationMode(ENCODER_QEPA_GPIO, GPIO_QUAL_ASYNC);
    GPIO_setQualificationMode(ENCODER_QEPB_GPIO, GPIO_QUAL_ASYNC);
    GPIO_setQualificationMode(ENCODER_QEPI_GPIO, GPIO_QUAL_ASYNC);

    XBAR_setInputPin(INPUTXBAR_BASE, XBAR_INPUT37, ENCODER_QEPA_GPIO);
    XBAR_setInputPin(INPUTXBAR_BASE, XBAR_INPUT38, ENCODER_QEPB_GPIO);
    XBAR_setInputPin(INPUTXBAR_BASE, XBAR_INPUT39, ENCODER_QEPI_GPIO);

}

void Encoder_init(void)
{
    const EQEP_SourceSelect sourceConfig =
    {
        EQEP_SOURCE_INPUTXBAR,
        EQEP_SOURCE_INPUTXBAR,
        EQEP_SOURCE_INPUTXBAR
    };

    configureEncoderPins();

    EQEP_disableModule(EQEP2_BASE);
    EQEP_selectSource(EQEP2_BASE, sourceConfig);
    EQEP_setDecoderConfig(EQEP2_BASE, EQEP_CONFIG_QUADRATURE |
                                      EQEP_CONFIG_2X_RESOLUTION |
                                      EQEP_CONFIG_NO_SWAP);
    EQEP_setPositionCounterConfig(EQEP2_BASE, EQEP_POSITION_RESET_MAX_POS,
                                  ENCODER_MAX_POSITION);
    EQEP_setInitialPosition(EQEP2_BASE, 0U);
    EQEP_setPositionInitMode(EQEP2_BASE, EQEP_INIT_DO_NOTHING);
    EQEP_setPosition(EQEP2_BASE, 0U);
    EQEP_setLatchMode(EQEP2_BASE, EQEP_LATCH_UNIT_TIME_OUT);
    EQEP_enableUnitTimer(EQEP2_BASE, ENCODER_UNIT_TIMER_PERIOD);
    EQEP_enableModule(EQEP2_BASE);
}

void Encoder_updateState(void)
{
    g_motorControlState.encoder_position = EQEP_getPosition(EQEP2_BASE);
    g_motorControlState.encoder_position_latched =
        EQEP_getPositionLatch(EQEP2_BASE);
    g_motorControlState.encoder_direction =
        EQEP_getDirection(EQEP2_BASE) ? 1 : -1;
}
