namespace io {

bool errored(auto const &handle_container) {
  return handle_container.handle < 0;
}

struct Pi {
  int handle;
  operator int() const { return this->handle; }

  Pi(Pi const &) = delete;
  Pi & operator=(Pi const &) = delete;

  Pi(char const *addr = nullptr, char const *port = nullptr) :
      handle{pigpio_start(addr, port)} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
        char const *port_env{std::getenv("PIGPIO_PORT")};
        std::cerr << log_error_prefix
          << "connecting to pigpio daemon at "
          << "address " << (addr ? addr : "localhost")
          << ", port " << (port ? port : (port_env ? port_env : "8888"))
          << ": " << pigpio_error(this->handle) << std::endl;
    }
    //std::cout << "Pi constructed at " << this << std::endl;
  }

  Pi(Pi&& that) : handle{that.handle} {
    that.handle = PI_BAD_HANDLE;
    //std::cout << "Pi moved from " << &that << " to " << this << std::endl;
  }

  ~Pi() {
    if (this->handle >= 0) pigpio_stop(this->handle);
    //std::cout << "Pi destructed at " << this << std::endl;
  }
};

struct I2C {
  int handle;
  int const pi_handle;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  I2C(I2C const &) = delete;
  I2C & operator=(I2C const &) = delete;

  I2C(int const pi_handle, unsigned const bus,
      unsigned const addr, unsigned const flags = 0u) : 
      handle{i2c_open(pi_handle, bus, addr, flags)}, pi_handle{pi_handle} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
      auto const original_flags{std::cerr.flags()};
      std::cerr << log_error_prefix
        << "opening I2C device on "
        << "Pi " << pi_handle
        << std::hex << std::showbase
        << ", bus " << bus
        << ", address " << addr
        << ", flags " << flags
        << ": " << pigpio_error(this->handle) << std::endl;
      std::cerr.flags(original_flags);
    }
    //std::cout << "I2C constructed at " << this << std::endl;
  }

  I2C(I2C&& that) : handle{that.handle}, pi_handle{that.pi_handle} {
    that.handle = PI_BAD_HANDLE;
    //std::cout << "I2C moved from " << &that << " to " << this << std::endl;
  }

  ~I2C() {
    if (this->handle >= 0) {
      int const response{i2c_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix << "closing I2C device on "
        << "Pi " << pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "I2C destructed at " << this << std::endl;
  }
};

struct Serial {
  int handle;
  int const pi_handle;
  std::string const tty;
  operator unsigned() const { return static_cast<unsigned>(this->handle); }

  Serial(Serial const &) = delete;
  Serial & operator=(Serial const &) = delete;

  Serial(int const pi_handle, char * const tty, unsigned const baud_rate,
      unsigned const flags = 0u) :
      handle{serial_open(pi_handle, tty, baud_rate, flags)},
      pi_handle{pi_handle}, tty{tty} {
    if constexpr (cc::log_errors) if (this->handle < 0) {
      std::cerr << log_error_prefix
        << "opening " << tty
        << " on Pi " << pi_handle
        << ", baud rate " << baud_rate
        << ", flags " << flags
        << ": " << pigpio_error(this->handle) << std::endl;
    }
    //std::cout << "Serial constructed at " << this << std::endl;
  }

  Serial(Serial&& that) : handle{that.handle}, pi_handle{that.pi_handle},
      tty{std::move(that.tty)}{
    that.handle = PI_BAD_HANDLE;
    //std::cout << "Serial moved from " << &that << " to " << this << std::endl;
  }

  ~Serial() {
    if (this->handle >= 0) {
      int const response{serial_close(this->pi_handle, this->handle)};
      if constexpr (cc::log_errors) if (response < 0) std::cerr
        << log_error_prefix
        << "closing " << this->tty
        << " on Pi " << this->pi_handle
        << ": " << pigpio_error(response) << std::endl;
    }
    //std::cout << "Serial destructed at " << this << std::endl;
  }
};

struct DHT {
  DHTXXD_t * dht;
  operator DHTXXD_t * () const { return this->dht; }

  DHT(DHT const &) = delete;
  DHT & operator=(DHT const &) = delete;

  DHT(int const pi_handle, int const gpio_index, int const dht_model = DHTAUTO,
      DHTXXD_CB_t callback = nullptr) :
      dht{DHTXXD(pi_handle, gpio_index, dht_model, callback)} {
    //std::cout << "DHT constructed at " << this << std::endl;
  }

  DHT(DHT&& that) : dht{that.dht} {
    that.dht = nullptr;
    //std::cout << "DHT moved from " << &that << " to " << this << std::endl;
  }

  ~DHT() {
    if (this->dht) DHTXXD_cancel(this->dht);
    //std::cout << "DHT destructed at " << this << std::endl;
  }
};

bool errored(DHT const &) { return false; }

struct LPD433Receiver {
  _433D_rx_t * lpd433_receiver;
  operator _433D_rx_t * () const { return this->lpd433_receiver; }

  LPD433Receiver(LPD433Receiver const &) = delete;
  LPD433Receiver & operator=(LPD433Receiver const &) = delete;

  LPD433Receiver(int const pi_handle, int const gpio_index,
      _433D_rx_CB_t callback = nullptr) :
      lpd433_receiver{_433D_rx(pi_handle, gpio_index, callback)} {
    //std::cout << "LPD433Receiver constructed at " << this << std::endl;
  }

  LPD433Receiver(LPD433Receiver&& that) :
      lpd433_receiver{that.lpd433_receiver} {
    that.lpd433_receiver = nullptr;
    //std::cout << "LPD433Receiver moved from " << &that << " to " << this
    //  << std::endl;
  }

  ~LPD433Receiver() {
    if (this->lpd433_receiver) _433D_rx_cancel(this->lpd433_receiver);
    //std::cout << "LPD433Receiver destructed at " << this << std::endl;
  }
};

bool errored(LPD433Receiver const &) { return false; }

struct LPD433Transmitter {
  _433D_tx_t * lpd433_transmitter;
  operator _433D_tx_t * () const { return this->lpd433_transmitter; }

  LPD433Transmitter(LPD433Transmitter const &) = delete;
  LPD433Transmitter & operator=(LPD433Transmitter const &) = delete;

  LPD433Transmitter(int const pi_handle, int const gpio_index) :
      lpd433_transmitter{_433D_tx(pi_handle, gpio_index)} {
    //std::cout << "LPD433Transmitter constructed at " << this << std::endl;
  }

  LPD433Transmitter(LPD433Transmitter&& that) :
      lpd433_transmitter{that.lpd433_transmitter} {
    that.lpd433_transmitter = nullptr;
    //std::cout << "LPD433Transmitter moved from " << &that << " to " << this
    //  << std::endl;
  }

  ~LPD433Transmitter() {
    if (this->lpd433_transmitter) _433D_tx_cancel(this->lpd433_transmitter);
    //std::cout << "LPD433Transmitter destructed at " << this << std::endl;
  }
};

bool errored(LPD433Transmitter const &) { return false; }

auto create_i2c_reader(int const pi_handle, int const i2c_handle) {
  return [=](unsigned const reg){
    int const response{i2c_read_byte_data(pi_handle, i2c_handle, reg)};
    if (response < 0) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "reading from "
        << "Pi " << pi_handle
        << ", I2C " << i2c_handle
        << ", register " << reg
        << ": " << pigpio_error(response) << std::endl;
      return std::optional<std::uint8_t>{};
    } else {
      return std::optional<std::uint8_t>{response & std::uint8_t{0xffu}};
    }
  };
}

template <typename T0, typename T1 = std::uint8_t>
bool get_flag(T0 const &status, T1 const &flag = 0xffu){
  return (status & flag) != 0u;
}
template <typename T0, typename T1 = std::uint8_t>
std::optional<bool> get_flag(
    std::optional<T0> const &status, T1 const &flag = 0xffu){
  return (status.has_value())
    ? std::optional<bool>{get_flag(status.value(), flag)}
    : std::optional<bool>{};
};

// Wrapper for the pigpio function of the same name (without leading underscore)
int _serial_data_available(Serial const &serial) {
  int const response{serial_data_available(serial.pi_handle, serial)};
  if constexpr (cc::log_errors) if (response < 0) std::cerr << log_error_prefix
    << "querying " << serial.tty
    << " on Pi " << serial.pi_handle
    << ": " << pigpio_error(response) << std::endl;
  return response;
}

// Wrapper for the pigpio function of the same name (without leading underscore)
int _serial_read_byte(Serial const &serial) {
  int const response{serial_read_byte(serial.pi_handle, serial)};
  if constexpr (cc::log_errors) if (response < 0) std::cerr << log_error_prefix
    << "reading byte from " << serial.tty
    << " on Pi " << serial.pi_handle
    << ": " << pigpio_error(response) << std::endl;
  return response;
}

// Wrapper for the pigpio function of the same name (without leading underscore)
int _serial_read(Serial const &serial, char * const buf, unsigned const count) {
  int const response{serial_read(serial.pi_handle, serial, buf, count)};
  if constexpr (cc::log_errors) if (response < 0) std::cerr << log_error_prefix
    << "reading from " << serial.tty
    << " on Pi " << serial.pi_handle
    << ": " << pigpio_error(response) << std::endl;
  return response;
}

// Wrapper for the pigpio function of the same name (without leading underscore)
int _serial_write(Serial const &serial, char * const buf,
    unsigned const count) {
  int const response{serial_write(serial.pi_handle, serial, buf, count)};
  if constexpr (cc::log_errors) if (response < 0) std::cerr << log_error_prefix
    << "writing to " << serial.tty
    << " on Pi " << serial.pi_handle
    << ": " << pigpio_error(response) << std::endl;
  return response;
}

// Empties all bytes buffered for reading from the given serial port
int serial_flush(Serial const &serial) {
  int const response{_serial_data_available(serial)};
  bool success{true};
  for (int i{0}; i < response; ++i) {
    int const response{_serial_read_byte(serial)};
    if (response < 0) success = false;
  }
  if constexpr (cc::log_info) if (response > 0 and success) std::cerr
    << log_info_prefix << "successfully flushed " << response << " bytes from "
    << serial.tty << std::endl;
  return response;
}

// Checks periodically until requested byte count is available and then reads
template <class Rep0, class Period0, class Rep1, class Period1>
std::optional<int> serial_wait_read(Serial const &serial, char * const buf,
    unsigned const count, std::chrono::duration<Rep0, Period0> const &timeout,
    std::chrono::duration<Rep1, Period1> const &interval) {
  std::size_t const
    max_intervals_to_wait{static_cast<std::size_t>(timeout / interval)};
  for (std::size_t i{0}; i < max_intervals_to_wait; ++i) {
    int const response{_serial_data_available(serial)};
    if (response >= static_cast<int>(count))
      return _serial_read(serial, buf, count);
    std::this_thread::sleep_for(interval);
  }

  if constexpr (cc::log_errors) {
    using T = double;
    T const timeout_in_seconds{static_cast<T>(timeout.count()) *
      static_cast<T>(Period0::num) / static_cast<T>(Period0::den)};
    std::cerr << log_error_prefix
      << "reading from " << serial.tty
      << " on Pi " << serial.pi_handle
      << ": " << "timeout after " << max_intervals_to_wait
      << " retries in " << timeout_in_seconds << "s" << std::endl;
  }
  return {};
}

char mhz19_checksum(auto const packet) {
  char checksum{0x00};
  for(std::size_t i{1}; i < 8; ++i) checksum += packet[i];
  return (0xff - checksum) + 0x01;
}

int mhz19_send(Serial const &serial, auto const packet) {
  std::array<char, 9> buf{{packet[0], packet[1], packet[2], packet[3],
    packet[4], packet[5], packet[6], packet[7], mhz19_checksum(packet)}};
  return _serial_write(serial, buf.data(), buf.size());
}

template <typename Rep0 = decltype(cc::mhz19_receive_timeout_default)::rep,
  typename Period0 = decltype(cc::mhz19_receive_timeout_default)::period,
  typename Rep1 = decltype(cc::mhz19_receive_interval_default)::rep,
  typename Period1 = decltype(cc::mhz19_receive_interval_default)::period>
std::optional<std::array<std::uint8_t, 8>> mhz19_receive(Serial const &serial,
    std::chrono::duration<Rep0, Period0> const &timeout =
      cc::mhz19_receive_timeout_default,
    std::chrono::duration<Rep1, Period1> const &interval =
      cc::mhz19_receive_interval_default) {
  std::array<char, 9> buf{};
  auto const response_opt{
    serial_wait_read(serial, buf.data(), buf.size(), timeout, interval)};

  if (not response_opt.has_value()) return {};
  auto const response{response_opt.value()};

  if (response < 0) return {};

  if (response >= 0 and response < static_cast<int>(buf.size())) {
    if constexpr (cc::log_errors) std::cerr << log_error_prefix
      << "receiving packet from MH-Z19 via " << serial.tty
      << " on Pi " << serial.pi_handle
      << ": expected to read 9 bytes, got " << response << std::endl;
    return {};
  }

  if (buf[8] != mhz19_checksum(buf)) {
    if constexpr (cc::log_info) std::cerr << log_info_prefix
      << "wrong checksum in packet from MH-Z19 via " << serial.tty
      << " on Pi " << serial.pi_handle << std::endl;
    return {};
  }

  return {{{buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]}}};
}

std::thread lpd433_send_oneshot(auto const &pi, int const gpio_index,
    std::vector<std::uint64_t> const &codes, int const n_bits,
    int const n_repeats, int const intercode_gap, int const pulse_length_short,
    int const pulse_length_long, bool const wait = true) {
  auto const lpd433_send_oneshotter_body{[&](){
    io::LPD433Transmitter const lpd433_transmitter{pi, gpio_index};
    _433D_tx_set_bits(lpd433_transmitter, n_bits);
    _433D_tx_set_repeats(lpd433_transmitter, n_repeats);
    _433D_tx_set_timings(lpd433_transmitter, intercode_gap,
      pulse_length_short, pulse_length_long);
    for (auto const &code : codes)
      _433D_tx_send(lpd433_transmitter, code);
  }};

  auto const lpd433_send_oneshotter{[&](){
    if constexpr (cc::log_info) {
    std::chrono::high_resolution_clock const clock{};
    auto tic{clock.now()};
    lpd433_send_oneshotter_body();
    auto toc{clock.now()};
    auto const t{
      std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count()};
    std::cerr << log_info_prefix << "Sending " << codes.size()
      << " LPD433 code(s) with " << n_bits << " bit(s), " << n_repeats
      << " repetition(s), and timings (gap, short, " "long) of "
      << intercode_gap << "µs, " << pulse_length_short << "µs, and "
      << pulse_length_long << "µs took " << t << "ms." << std::endl;
    } else lpd433_send_oneshotter_body();
  }};

  std::thread t{lpd433_send_oneshotter};
  if (wait) t.join();
  return t;
}

std::thread buzz_oneshot(auto const &pi, int const gpio_index,
    float const t_seconds = cc::buzz_t_seconds_default,
    float const f_hertz = cc::buzz_f_hertz_default,
    float const pulse_width = cc::buzz_pulse_width_default,
    bool const wait = true) {
  unsigned range{40000u}, frequency{static_cast<unsigned>(f_hertz)};
  unsigned dutycycle{
    static_cast<unsigned>(pulse_width * static_cast<float>(range))};
  std::chrono::duration<float> duration{t_seconds};

  auto const buzz_oneshotter{[&](){
    int response;

    response = set_PWM_range(pi, gpio_index, range);
    if (response < 0) {
      if (cc::log_errors) std::cerr << log_error_prefix
        << "setting PWM range: "
        << pigpio_error(response) << std::endl;
      return;
    }

    response = set_PWM_frequency(pi, gpio_index, frequency);
    if (response < 0) {
      if (cc::log_errors) std::cerr << log_error_prefix
        << "setting PWM frequency: "
        << pigpio_error(response) << std::endl;
      return;
    }

    response = set_PWM_dutycycle(pi, gpio_index, dutycycle);
    if (response < 0) {
      if (cc::log_errors) std::cerr << log_error_prefix
        << "starting PWM: "
        << pigpio_error(response) << std::endl;
      return;
    }

    std::this_thread::sleep_for(duration);

    response = set_PWM_dutycycle(pi, gpio_index, 0u);
    if (response < 0) {
      if (cc::log_errors) std::cerr << log_error_prefix
        << "stopping PWM: "
        << pigpio_error(response) << std::endl;
      return;
    }
  }};
  std::thread t{buzz_oneshotter};
  if (wait) t.join();
  return t;
}

} // namespace io

