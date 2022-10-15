#pragma once

#include "FreeRTOS.h"
#include "semphr.h"
#include <cstring>

#ifdef TESTING
void test_utils();
#endif

namespace utils {

  /// @brief RAII lock for RTOS mutex
  struct Lock {
  private:
    const SemaphoreHandle_t mtx_{ nullptr };
    bool is_locked_{ true };

  public:
    Lock(SemaphoreHandle_t mtx) : mtx_(mtx) {
      xSemaphoreTake(mtx_, portMAX_DELAY);
    }
    ~Lock() {
      if (is_locked_) {
        xSemaphoreGive(mtx_);
      }
    }

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    // this will always be initialized, not sure why the warning is shown
    Lock(Lock&& other) : mtx_(other.mtx_) {
#pragma GCC diagnostic pop
      other.is_locked_ = false;
    }
  };

  /// @brief RAII lock for RTOS mutex from ISR context
  struct LockISR {
  private:
    const SemaphoreHandle_t mtx_{};
    bool is_locked_ = false;

  public:
    LockISR(SemaphoreHandle_t mtx) : mtx_(mtx) {
      is_locked_ = pdTRUE == xSemaphoreTakeFromISR(mtx_, nullptr);
    }
    ~LockISR() {
      if (is_locked_) {
        xSemaphoreGiveFromISR(mtx_, nullptr);
      }
    }

    [[nodiscard]] bool operator()() const {
      return is_locked_;
    }
  };


  template <class T, class U, class V>
  constexpr inline T constrain(T val, U low, V high) {
    if (val <= low) {
      return low;
    } else if (val >= high) {
      return high;
    } else {
      return val;
    }
  }


  template <class T, class K, class L>
  inline constexpr bool within(T val, K low, L high) {
    return (val >= low) && (val <= high);
  }


  template <class T, class K, class L>
  inline constexpr bool between(T val, K low, L high) {
    return (val > low) && (val < high);
  }

  inline constexpr bool elapsed(uint32_t now, uint32_t next) {
    return static_cast<int32_t>(now - next) >= 0;
  }

  template <class T>
  inline constexpr T sign(T val) {
    if (val > 0) return 1;
    if (val < 0) return -1;
    return 0;
  }


  template <class T>
  inline T mem2T(uint8_t* mem) {
    T ret;
    memcpy(&ret, mem, sizeof(T));
    return ret;
  }

  inline constexpr uint32_t crc32mpeg2(const uint8_t* buf, size_t len, uint32_t crc = 0xffffffff) {
    for (size_t i = 0; i < len; ++i) {
      crc ^= buf[i] << 24;
      for (int j = 0; j < 8; ++j) {
        if ((crc & 0x80000000) == 0) {
          crc = crc << 1;
        } else {
          crc = (crc << 1) ^ 0x104c11db7;
        }
      }
    }
    return crc;
  }

}  // namespace utils
