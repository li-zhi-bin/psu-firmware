/*
 * EEZ PSU Firmware
 * Copyright (C) 2016-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "eez/psu/psu.h"

#if OPTION_DISPLAY

#include "eez/psu/persist_conf.h"
#include "eez/psu/sound.h"
#include "eez/psu/calibration.h"
#include "eez/psu/channel_dispatcher.h"
#include "eez/psu/trigger.h"

#include "eez/psu/gui/psu.h"
#include "eez/psu/gui/data.h"
#include "eez/psu/gui/password.h"
#include "eez/psu/gui/calibration.h"
#include "eez/psu/gui/keypad.h"
#include "eez/psu/gui/numeric_keypad.h"

namespace eez {
namespace psu {
namespace gui {
namespace calibration {

int g_stepNum;
void (*g_stopCallback)();

void showCurrentStep();

////////////////////////////////////////////////////////////////////////////////

void onStartPasswordOk() {
    g_channel->outputEnable(false);

    g_channel->clearProtection();
    g_channel->prot_conf.flags.u_state = 0;
    g_channel->prot_conf.flags.i_state = 0;
    g_channel->prot_conf.flags.p_state = 0;

    if (g_channel->getFeatures() & CH_FEATURE_RPROG) {
        g_channel->remoteProgrammingEnable(false);
    }
    if (g_channel->getFeatures() & CH_FEATURE_LRIPPLE) {
        channel_dispatcher::lowRippleEnable(*g_channel, false);
        channel_dispatcher::lowRippleAutoEnable(*g_channel, false);
    }

    g_channel->outputEnable(true);

    trigger::abort();

    psu::calibration::start(g_channel);

    g_stepNum = 0;
    showCurrentStep();
}

void start() {
    checkPassword("Password: ", persist_conf::devConf.calibration_password, onStartPasswordOk);
}

data::Value getLevelValue() {
    if (g_stepNum < 3) {
        return MakeValue(psu::calibration::getVoltage().getLevelValue(), UNIT_VOLT, g_channel->index-1);
    }
    return MakeValue(psu::calibration::getCurrent().getLevelValue(), UNIT_AMPER, g_channel->index-1);
}

void showCurrentStep() {
    psu::calibration::resetChannelToZero();

    if (g_stepNum < MAX_STEP_NUM) {
        switch (g_stepNum) {
        case 0: psu::calibration::getVoltage().setLevel(psu::calibration::LEVEL_MIN); break;
        case 1: psu::calibration::getVoltage().setLevel(psu::calibration::LEVEL_MID); break;
        case 2: psu::calibration::getVoltage().setLevel(psu::calibration::LEVEL_MAX); break;
        case 3: psu::calibration::selectCurrentRange(0); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MIN); break;
        case 4: psu::calibration::selectCurrentRange(0); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MID); break;
        case 5: psu::calibration::selectCurrentRange(0); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MAX); break;
        case 6: psu::calibration::selectCurrentRange(1); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MIN); break;
        case 7: psu::calibration::selectCurrentRange(1); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MID); break;
        case 8: psu::calibration::selectCurrentRange(1); psu::calibration::getCurrent().setLevel(psu::calibration::LEVEL_MAX); break;
        }

        replacePage(PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_STEP);
    } else {
        replacePage(PAGE_ID_SYS_SETTINGS_CAL_CH_WIZ_FINISH);
     }
}

psu::calibration::Value *getCalibrationValue() {
    if (g_stepNum < 3) {
        return &psu::calibration::getVoltage();
    }
    return &psu::calibration::getCurrent();
}


void onSetLevelOk(float value) {
    getCalibrationValue()->setDacValue(value);

    popPage();
}

void setLevelValue() {
    data::Value levelValue = getLevelValue();

    NumericKeypadOptions options;

    options.channelIndex = psu::calibration::getCalibrationChannel().index - 1;

    options.editValueUnit = levelValue.getUnit();

    if (levelValue.getUnit() == UNIT_VOLT) {
        options.min = g_channel->u.min;
        options.max = g_channel->u.max;
    } else {
        options.min = g_channel->i.min;
        options.max = g_channel->i.max;
    }

    options.def = 0;

    options.flags.signButtonEnabled = true;
    options.flags.dotButtonEnabled = true;

    NumericKeypad *numericKeypad = NumericKeypad::start(0, levelValue, options, onSetLevelOk, showCurrentStep);

    if (g_stepNum == 0 || g_stepNum == 3 || (g_stepNum >= 6 && g_stepNum <= 8)) {
        numericKeypad->switchToMilli();
    }
}

void onSetOk(float value) {
    psu::calibration::Value *calibrationValue = getCalibrationValue();

    float dac = calibrationValue->getDacValue();
    float adc = calibrationValue->getAdcValue();
    if (calibrationValue->checkRange(dac, value, adc)) {
        calibrationValue->setData(dac, value, adc);

        popPage();
        nextStep();
    } else {
        errorMessageP("Value out of range!");
    }
}

void onSetRemarkOk(char *remark) {
    psu::calibration::setRemark(remark, strlen(remark));
    popPage();
    if (g_stepNum < MAX_STEP_NUM - 1) {
        nextStep();
    } else {
        int16_t scpiErr;
        if (psu::calibration::canSave(scpiErr)) {
            nextStep();
        } else {
            showCurrentStep();
        }
    }
}

void set() {
    if (!psu::calibration::isEnabled()) {
        setPage(PAGE_ID_MAIN);
        return;
    }

    if (g_stepNum < MAX_STEP_NUM - 1) {
        if (g_stepNum < 3) {
            psu::calibration::getVoltage().setLevelValue();
        } else {
            psu::calibration::getCurrent().setLevelValue();
        }

        psu::calibration::Value *calibrationValue = getCalibrationValue();

        NumericKeypadOptions options;

        options.channelIndex = psu::calibration::getCalibrationChannel().index - 1;

        if (calibrationValue == &psu::calibration::getVoltage()) {
            options.editValueUnit = UNIT_VOLT;

            options.min = g_channel->u.min;
            options.max = g_channel->u.max;

        } else {
            options.editValueUnit = UNIT_AMPER;

            options.min = g_channel->i.min;
            options.max = g_channel->i.max;
        }

        options.def = 0;

        options.flags.signButtonEnabled = true;
        options.flags.dotButtonEnabled = true;

        NumericKeypad *numericKeypad = NumericKeypad::start(0, data::Value(), options, onSetOk, showCurrentStep);

        if (g_stepNum == 0 || g_stepNum == 3 || (g_stepNum >= 6 && g_stepNum <= 8)) {
            numericKeypad->switchToMilli();
        }
    } else if (g_stepNum == MAX_STEP_NUM - 1) {
        psu::calibration::resetChannelToZero();
        Keypad::startPush(0, psu::calibration::isRemarkSet() ? psu::calibration::getRemark() : 0, CALIBRATION_REMARK_MAX_LENGTH, false, onSetRemarkOk, popPage);
    }
}

void previousStep() {
    if (!psu::calibration::isEnabled()) {
        setPage(PAGE_ID_MAIN);
        return;
    }

    if (g_stepNum > 0) {
        --g_stepNum;
        if (g_stepNum == 8 && !psu::calibration::hasSupportForCurrentDualRange()) {
            g_stepNum = 5;
        }
        showCurrentStep();
    }
}

void nextStep() {
    if (!psu::calibration::isEnabled()) {
        setPage(PAGE_ID_MAIN);
        return;
    }

    if (g_stepNum == MAX_STEP_NUM - 1) {
        int16_t scpiErr;
        if (!psu::calibration::canSave(scpiErr)) {
            errorMessage(data::Cursor(psu::calibration::getCalibrationChannel().index - 1), data::MakeScpiErrorValue(scpiErr));
            return;
        }
    }

    ++g_stepNum;
    if (g_stepNum == 6 && !psu::calibration::hasSupportForCurrentDualRange()) {
        g_stepNum = 9;
    }
    showCurrentStep();
}

void save() {
    if (psu::calibration::save()) {
        psu::calibration::stop();
        infoMessageP("Calibration data saved!", popPage);
    } else {
        errorMessageP("Save failed!");
    }
}

void finishStop() {
    psu::calibration::stop();

    g_channel->outputEnable(false);

    g_channel->prot_conf.flags.u_state = g_channel->OVP_DEFAULT_STATE;
    g_channel->prot_conf.flags.i_state = g_channel->OCP_DEFAULT_STATE;
    g_channel->prot_conf.flags.p_state = g_channel->OPP_DEFAULT_STATE;

    (*g_stopCallback)();
}

void stop(void (*callback)()) {
    g_stopCallback = callback;
    areYouSure(finishStop);
}

void toggleEnable() {
    Channel &channel = g_channel ? *g_channel : Channel::get(g_foundWidgetAtDown.cursor.i);
    channel.calibrationEnable(!channel.isCalibrationEnabled());
}

}
}
}
} // namespace eez::psu::gui::calibration

#endif