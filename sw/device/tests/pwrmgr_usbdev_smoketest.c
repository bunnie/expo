// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// This is a DV specific pwrmgr usbdev smoketest that fakes usb activity.
// Using the phy drive function of usbdev, we present the usbdev is in suspend
// and thus trigger low power entry.
// Once the system enters low power entry, the incoming USBP/USBN lines are
// immediately different from what the wake module expects and thus causes
// a wakeup.
// This test just smoke checks the pwrmgr's ability to enter / exit low power
// and the usbdev's aon wake function that has been de-coupled from the main
// usbdev.

#include "dt/dt_pinmux.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_pwrmgr.h"
#include "sw/device/lib/dif/dif_usbdev.h"
#include "sw/device/lib/runtime/hart.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/testing/pwrmgr_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

static dif_pwrmgr_t pwrmgr;
static dif_usbdev_t usbdev;

static const dt_pwrmgr_t kPwrmgrDt = 0;
static_assert(kDtPwrmgrCount == 1, "this test expects a pwrmgr");
static const dt_pinmux_t kPinmuxDt = 0;
static_assert(kDtPinmuxCount == 1, "this test expects a pinmux");
static const dt_usbdev_t kUsbdevDt = 0;
static_assert(kDtUsbdevCount >= 1, "this test expects at least one usbdev");

OTTF_DEFINE_TEST_CONFIG();

static bool compare_wakeup_reasons(dif_pwrmgr_wakeup_reason_t lhs,
                                   dif_pwrmgr_wakeup_reason_t rhs) {
  return lhs.types == rhs.types && lhs.request_sources == rhs.request_sources;
}

bool test_main(void) {
  CHECK_DIF_OK(dif_pwrmgr_init_from_dt(kPwrmgrDt, &pwrmgr));
  CHECK_DIF_OK(dif_usbdev_init_from_dt(kUsbdevDt, &usbdev));

  dif_pwrmgr_request_sources_t wakeup_sources;
  CHECK_DIF_OK(dif_pwrmgr_find_request_source(
      &pwrmgr, kDifPwrmgrReqTypeWakeup, dt_pinmux_instance_id(kPinmuxDt),
      kDtPinmuxWakeupUsbWkupReq, &wakeup_sources));

  // Assuming the chip hasn't slept yet, wakeup reason should be empty.
  dif_pwrmgr_wakeup_reason_t wakeup_reason;
  CHECK_DIF_OK(dif_pwrmgr_wakeup_reason_get(&pwrmgr, &wakeup_reason));

  const dif_pwrmgr_wakeup_reason_t exp_por_wakeup_reason = {
      .types = 0,
      .request_sources = 0,
  };

  const dif_pwrmgr_wakeup_reason_t exp_test_wakeup_reason = {
      .types = kDifPwrmgrWakeupTypeRequest,
      .request_sources = wakeup_sources,
  };

  bool low_power_exit = false;
  if (compare_wakeup_reasons(wakeup_reason, exp_por_wakeup_reason)) {
    LOG_INFO("Powered up for the first time, begin test");
  } else if (compare_wakeup_reasons(wakeup_reason, exp_test_wakeup_reason)) {
    low_power_exit = true;
    LOG_INFO("USB wakeup detected");
  } else {
    LOG_ERROR("Unexpected wakeup reason (types: %02x, sources: %08x)",
              wakeup_reason.types, wakeup_reason.request_sources);
    return false;
  }

  // Fake low power entry through usb
  // Force wake detection module active
  if (!low_power_exit) {
    CHECK_DIF_OK(dif_usbdev_set_wake_enable(&usbdev, kDifToggleDisabled));
    dif_usbdev_phy_pins_drive_t pins = {
        .dp_pullup_en = true,
        .dn_pullup_en = false,
    };
    CHECK_DIF_OK(
        dif_usbdev_set_phy_pins_state(&usbdev, kDifToggleEnabled, pins));
    CHECK_DIF_OK(dif_usbdev_set_wake_enable(&usbdev, kDifToggleEnabled));

    // give the hardware a chance to recognize the wakeup values are the same
    busy_spin_micros(20);  // 20us

    // Enable low power on the next WFI with default settings.
    CHECK_STATUS_OK(pwrmgr_testutils_enable_low_power(
        &pwrmgr, wakeup_sources, kDifPwrmgrDomainOptionUsbClockInActivePower));

    // Enter low power mode.
    wait_for_interrupt();
  }

  return true;
}
