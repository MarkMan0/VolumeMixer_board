#include "comm_class.h"
#include "sem_lock.h"



void CommClass::init() {
  passert(hw_msg_);
  hw_msg_->init();
  flush_mtx_ = xSemaphoreCreateMutex();
  passert(flush_mtx_);
}

size_t CommClass::write(const uint8_t* data, size_t len) {
  if (tx_buffer_.free() < len) {
    return 0;
  }
  if (auto ptr = tx_buffer_.reserve(len)) {
    memcpy(ptr, data, len);
    tx_buffer_.commit();
  } else {
    for (size_t i = 0; i < len; ++i) {
      tx_buffer_.push(data[i]);
    }
  }
  notify_tx_task();
  return len;
}

size_t CommClass::write(const char* str) {
  return write(reinterpret_cast<const uint8_t*>(str), strlen(str) + 1);
}

size_t CommClass::write(uint8_t c) {
  if (tx_buffer_.is_full()) {
    return 0;
  }
  tx_buffer_.push(c);
  notify_tx_task();
  return 1;
}


size_t CommClass::available() const {
  return rx_buffer_.size();
}

size_t CommClass::wait_for(size_t n) const {
  uint32_t cnt = UART_TimingConfig::READ_MAX_RETRY;
  while (available() < n) {
    if (--cnt == 0) {
      return 0;
    }
    vTaskDelay(5);
  }
  return available();
}

size_t CommClass::read(uint8_t* buff, size_t max_len) {
  // TODO improve by copying continuous space
  auto sz = rx_buffer_.size();
  size_t i = 0;
  while (sz && i < max_len) {
    buff[i] = rx_buffer_.pop();
    --sz;
    ++i;
  }
  return i;
}

size_t CommClass::read(char* buff, size_t max_len) {
  return read(reinterpret_cast<uint8_t*>(buff), max_len);
}

void CommClass::empty_rx() {
  rx_buffer_.reset();
}

void CommClass::flush() {
  if (not hw_msg_->status()) {
    // not connected
    return;
  }
  utils::Lock lck(flush_mtx_);

  uint32_t cnt = UART_TimingConfig::WRITE_MAX_RETRY;
  while (auto n = tx_buffer_.size_cont()) {
    if (n == hw_msg_->transmit(&(tx_buffer_.peek()), n)) {
      tx_buffer_.pop(n);
    } else {
      if (--cnt == 0) {
        return;
      }
      vTaskDelay(UART_TimingConfig::WRITE_RETRY_TICKS);
    }
  }
}

void CommClass::send_task() {
  xTaskNotifyWait(0, UINT32_MAX, nullptr, portMAX_DELAY);
  flush();
}


void CommClass::set_tx_task(xTaskHandle h) {
  passert(h);
  tx_task_ = h;
}

void CommClass::notify_tx_task() {
  passert(tx_task_);
  if (tx_task_) {
    xTaskNotify(tx_task_, 0, eNoAction);
  }
}

void CommClass::receive(const void* buff, size_t len) {
  rx_buffer_.push(static_cast<const uint8_t*>(buff), len);
}