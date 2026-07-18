/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-FileCopyrightText: Paranoid Android
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler.xiaomi_sm8750"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>

#include <fstream>

#include "UdfpsHandler.h"

#define COMMAND_FOD_PRESS_STATUS 1
#define COMMAND_FOD_PRESS_X 2
#define COMMAND_FOD_PRESS_Y 3
#define PARAM_FOD_PRESSED 1
#define PARAM_FOD_RELEASED 0
#define QCOM_ACQUIRED_RETRY 28
#define QCOM_ACQUIRED_WAIT_FOR_FINGER_UP 201
#define QCOM_ACQUIRED_CAPTURE_COMPLETE 202

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

namespace {

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

}  // anonymous namespace

class SM8750UdfpsHandler : public UdfpsHandler {
  public:
    void init(fingerprint_device_t* device) {
        mDevice = device;
        mIsQcomUs = android::base::GetProperty("persist.vendor.sys.fp.vendor", "") == "qcom_us";
        LOG(INFO) << __func__ << ": qcom_us=" << mIsQcomUs;
    }

    void onFingerDown(uint32_t x, uint32_t y, float /*minor*/, float /*major*/) {
        LOG(DEBUG) << __func__ << "x: " << x << ", y: " << y;
        if (!mDevice || !mDevice->extCmd) {
            LOG(ERROR) << __func__ << ": fingerprint HAL extCmd is unavailable";
            return;
        }
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, x);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, y);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS, PARAM_FOD_PRESSED);
    }

    void onFingerUp() {
        LOG(DEBUG) << __func__;
        if (!mDevice || !mDevice->extCmd) {
            LOG(ERROR) << __func__ << ": fingerprint HAL extCmd is unavailable";
            return;
        }
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, 0);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, 0);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS, PARAM_FOD_RELEASED);
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        LOG(DEBUG) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
        if (static_cast<AcquiredInfo>(result) == AcquiredInfo::VENDOR &&
            ((mIsQcomUs && vendorCode == QCOM_ACQUIRED_RETRY) ||
             vendorCode == QCOM_ACQUIRED_WAIT_FOR_FINGER_UP ||
             vendorCode == QCOM_ACQUIRED_CAPTURE_COMPLETE)) {
            onFingerUp();
        }
    }

    void cancel() {
        LOG(INFO) << __func__;
        onFingerUp();
    }

  private:
    fingerprint_device_t* mDevice = nullptr;
    bool mIsQcomUs = false;
};

static UdfpsHandler* create() {
    return new SM8750UdfpsHandler();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
        .create = create,
        .destroy = destroy,
};
