#include "utils/logger.h"
#include <coreinit/bsp.h>
#include <mocha/mocha.h>
#include <wups.h>
#include <wups/button_combo/api.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemButtonCombo.h>

#include <forward_list>

#include <malloc.h>

WUPS_PLUGIN_NAME("EjectDisc");
WUPS_PLUGIN_DESCRIPTION("Adds a bind to eject the current game disc.");
WUPS_PLUGIN_VERSION("v1.0.0");
WUPS_PLUGIN_AUTHOR("Lynx64, acikek");
WUPS_PLUGIN_LICENSE("GPLv3");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("eject_disc");

#define ENABLED_CONFIG_ID     "enabled"
#define EJECT_COMBO_CONFIG_ID "ejectCombo"

bool ENABLED_DEFAULT                        = true;
WUPSButtonCombo_Buttons EJECT_COMBO_DEFAULT = WUPS_BUTTON_COMBO_BUTTON_DOWN | WUPS_BUTTON_COMBO_BUTTON_PLUS | WUPS_BUTTON_COMBO_BUTTON_MINUS;

static bool sEnabled;
static uint32_t sEjectCombo;

static std::forward_list<WUPSButtonComboAPI::ButtonCombo> sButtonComboInstances;

static WUPSButtonCombo_ComboHandle sEjectHandle(nullptr);

void enabledChanged(ConfigItemBoolean *item, bool newValue) {
    sEnabled = newValue;
    WUPSStorageAPI::Store(item->identifier, newValue);
    DEBUG_FUNCTION_LINE_INFO("Enabled set to %s", newValue ? "true" : "false");
}

void comboChanged(ConfigItemButtonCombo *item, uint32_t newValue) {
    sEjectCombo = newValue;
    WUPSStorageAPI::Store(item->identifier, newValue);
    DEBUG_FUNCTION_LINE_INFO("Eject combo set to %d", newValue);
}

WUPSConfigAPICallbackStatus configMenuOpened(WUPSConfigCategoryHandle rootHandle) {
    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);
    try {
        root.add(WUPSConfigItemBoolean::Create(ENABLED_CONFIG_ID, "Enabled", ENABLED_DEFAULT, sEnabled, &enabledChanged));
        root.add(WUPSConfigItemButtonCombo::Create(EJECT_COMBO_CONFIG_ID, "Eject", EJECT_COMBO_DEFAULT, sEjectHandle, &comboChanged));
    } catch (std::exception &e) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create EjectDisc config menu: %s", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }
    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void configMenuClosed() {
    WUPSStorageAPI::SaveStorage();
}

// Source: https://github.com/Lynx64/EjectDisc/blob/main/src/main.cpp
void giveEjectRequestPpcPermissions() {
    // entity & attribute struct: https://github.com/NWPlayer123/IOSU/blob/master/ios_bsp/libraries/bsp_entity/include/bsp_entity.h
    // SMC entity is at 0xE6040D94
    // SMC attributes array: 0xE6044364
    uint32_t permissions = 0;
    // +44 for 2nd attribute (which is EjectRequest), +8 for permissions
    Mocha_IOSUKernelRead32(0xE6044364 + 44 + 8, &permissions);
    // by default EjectRequest has perms 0xFF (BSP_PERMISSIONS_IOS)
    Mocha_IOSUKernelWrite32(0xE6044364 + 44 + 8, permissions | 0xF00); // BSP_PERMISSIONS_PPC_USER
}

void comboPressed() {
    DEBUG_FUNCTION_LINE_INFO("Eject combo pressed");
    if (!sEnabled) {
        return;
    }
    DEBUG_FUNCTION_LINE_INFO("Ejecting...");
    giveEjectRequestPpcPermissions();
    uint32_t request = 1;
    bspWrite("SMC", 0, "EjectRequest", 4, &request);
}

INITIALIZE_PLUGIN() {
    initLogging();
    DEBUG_FUNCTION_LINE("Initializing EjectDisc...");
    WUPSConfigAPIOptionsV1 options = {.name = "EjectDisc"};
    if (WUPSConfigAPI_Init(options, configMenuOpened, configMenuClosed) != WUPSCONFIG_API_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to initialize EjectDisc config");
    }
    WUPSStorageAPI::GetOrStoreDefault(ENABLED_CONFIG_ID, sEnabled, ENABLED_DEFAULT);
    WUPSStorageAPI::GetOrStoreDefault<uint32_t>(EJECT_COMBO_CONFIG_ID, sEjectCombo, EJECT_COMBO_DEFAULT);
    try {
        WUPSButtonCombo_ComboStatus comboStatus;
        WUPSButtonComboAPI::ButtonCombo ejectCombo = WUPSButtonComboAPI::CreateComboHold(
                "Eject disc button combo",
                static_cast<WUPSButtonCombo_Buttons>(sEjectCombo),
                500,
                [](const WUPSButtonCombo_ControllerTypes triggeredBy, WUPSButtonCombo_ComboHandle, void *) {
                    comboPressed();
                },
                nullptr,
                comboStatus);
        sEjectHandle = ejectCombo.getHandle();
        sButtonComboInstances.emplace_front(std::move(ejectCombo));
    } catch (std::exception &e) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create Eject combo: %s", e.what());
    }
    deinitLogging();
}

DEINITIALIZE_PLUGIN() {
    sButtonComboInstances.clear();
}

ON_APPLICATION_START() {
    initLogging();
    Mocha_InitLibrary();
}

ON_APPLICATION_ENDS() {
    deinitLogging();
    Mocha_DeInitLibrary();
}
