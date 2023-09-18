namespace sensors {
  enum struct WriteFormat { csv, toml };

  std::string write_format_ext(WriteFormat const wf) {
    if (wf == WriteFormat::csv) return "csv";
    else if (wf == WriteFormat::toml) return "toml";
    else throw std::logic_error("`wf` must be one of the defined enum values");
  }
}

#include "sensors.generated.cpp"

namespace sensors {
  // IO setup and sampling functions, written here manually, because
  // machine-generating them would involve a lot of complexity for little gain
  // at my scale of operation.  However, I am generating their declarations, as
  // those are useful in some of the other generated code.

  // One design choice to be made here is how to handle the aggregation of time
  // values if the dependent variables are missing. For a sensor like the DHT22,
  // which has two dependent variables total which always fail together, it
  // would make sense to not include the time points of failed readings, as
  // taking the mean of the time points that are left would provide a better
  // precision for the actual time of the aggregated reading. Meanwhile, for
  // something like the SensorHub, where it's common to have most readings not
  // fail and only some occasionally fail, I would have to provide an
  // aggregated time point for each individual reading to achieve the same
  // precision. So I think I'll make this choice for each sampling function
  // individually, depending on whether all readings always fail in sync or not.

  auto setup_sensor_io(auto const &) { return nullptr; }

  sensor sample_sensor(auto const &clock, auto const &) {
    // NOTE: A design choice I made back when I first started the sensor-logging
    // repository was to use Unix time (as in seconds since epoch) to timestamp
    // each measurement. This function assumes that the `clock` measures Unix
    // time. In C++20, the `std::chrono::system_clock` is guaranteed to be Unix
    // time.
    auto const timestamp{std::chrono::duration_cast<cc::timestamp_duration_t>(
      clock.now().time_since_epoch())};
    return sensor{std::optional{timestamp}};
  }

  auto sample_sensor(auto const &clock) { return sample_sensor(clock, nullptr); }

  auto setup_sensorhub_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::I2C(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::I2C(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  sensorhub sample_sensorhub(auto const &clock, auto const &i2c) {
    unsigned constexpr reg_ntc_temperature   {0x01u};
    unsigned constexpr reg_brightness_0      {0x02u};
    unsigned constexpr reg_brightness_1      {0x03u};
    unsigned constexpr reg_status            {0x04u};
    unsigned constexpr reg_dht11_temperature {0x05u};
    unsigned constexpr reg_dht11_humidity    {0x06u};
    unsigned constexpr reg_dht11_error       {0x07u};
    unsigned constexpr reg_bmp280_temperature{0x08u};
    unsigned constexpr reg_bmp280_pressure_0 {0x09u};
    unsigned constexpr reg_bmp280_pressure_1 {0x0au};
    unsigned constexpr reg_bmp280_pressure_2 {0x0bu};
    unsigned constexpr reg_bmp280_error      {0x0cu};
    unsigned constexpr reg_motion            {0x0du};

    unsigned constexpr flag_ntc_overrange       {0x01u};
    unsigned constexpr flag_ntc_error           {0x02u};
    unsigned constexpr flag_brightness_overrange{0x04u};
    unsigned constexpr flag_brightness_error    {0x08u};

    auto const read{create_i2c_reader(i2c.pi_handle, i2c)};

    auto const status{read(reg_status)};

    std::optional<bool> const ntc_overrange{
      io::get_flag(status, flag_ntc_overrange)};
    std::optional<bool> const ntc_error{
      io::get_flag(status, flag_ntc_error)};
    std::optional<float> const ntc_temperature{[&](){
      std::optional<float> const ntc_temperature{read(reg_ntc_temperature)};
      return (not ntc_overrange.value_or(true) and not ntc_error.value_or(true))
        ? ntc_temperature : std::optional<float>{};
    }()};

    std::optional<bool> const dht11_error{io::get_flag(read(reg_dht11_error))};
    std::optional<float> const dht11_temperature{[&](){
      std::optional<float> const dht11_temperature{read(reg_dht11_temperature)};
      return (not dht11_error.value_or(true)) ? dht11_temperature
                                              : std::optional<float>{};
    }()};
    std::optional<float> const dht11_humidity{[&](){
      std::optional<float> const dht11_humidity{read(reg_dht11_humidity)};
      return (not dht11_error.value_or(true)) ? dht11_humidity
                                              : std::optional<float>{};
    }()};

    std::optional<bool> const bmp280_error{io::get_flag(read(reg_bmp280_error))};
    std::optional<float> const bmp280_temperature{[&](){
      std::optional<float> const bmp280_temperature{
        read(reg_bmp280_temperature)};
      return (not bmp280_error.value_or(true)) ? bmp280_temperature
                                               : std::optional<float>{};
    }()};
    std::optional<float> const bmp280_pressure{[&](){
      auto const bmp280_pressure_0{read(reg_bmp280_pressure_0)};
      auto const bmp280_pressure_1{read(reg_bmp280_pressure_1)};
      auto const bmp280_pressure_2{read(reg_bmp280_pressure_2)};
      if (bmp280_pressure_0.has_value() and
          bmp280_pressure_1.has_value() and
          bmp280_pressure_2.has_value() and
          not bmp280_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(bmp280_pressure_0.value()) <<  0u |
          static_cast<std::uint32_t>(bmp280_pressure_1.value()) <<  8u |
          static_cast<std::uint32_t>(bmp280_pressure_2.value()) << 16u};
      else return std::optional<float>{};
    }()};

    std::optional<bool> const brightness_overrange{
      io::get_flag(status, flag_brightness_overrange)};
    std::optional<bool> const brightness_error{
      io::get_flag(status, flag_brightness_error)};
    std::optional<float> const brightness{[&](){
      auto const brightness_0{read(reg_brightness_0)};
      auto const brightness_1{read(reg_brightness_1)};
      if (brightness_0.has_value() and
          brightness_1.has_value() and
          not brightness_overrange.value_or(true) and
          not brightness_error.value_or(true))
        return std::optional<float>{
          static_cast<std::uint32_t>(brightness_0.value()) << 0u |
          static_cast<std::uint32_t>(brightness_1.value()) << 8u};
      else return std::optional<float>{};
    }()};

    std::optional<float> const motion{util::optional_apply([](auto const x){
        return (x == 1) ? 1.f : 0.f;
      }, read(reg_motion))};

    return sensorhub{sample_sensor(clock),
      ntc_temperature,
      ntc_overrange,
      ntc_error,
      dht11_temperature,
      dht11_humidity,
      dht11_error,
      bmp280_temperature,
      bmp280_pressure,
      bmp280_error,
      brightness,
      brightness_overrange,
      brightness_error,
      motion};
  }

  auto setup_dht22_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::DHT(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::DHT(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  dht22 sample_dht22(auto const &clock, auto const &dht) {
    // The DHTXXD library provides two ways of reading data:
    // * Read periodically in separate thread and set a flag to indicate that
    //   new data are available. This flag is then reset when the data are
    //   retrieved.
    // * Read manually. In this case the flag (accessed through `DHTXXD_ready`)
    //   can be ignored. The comments in the library source files suggest not
    //   reading a DHT22 more often than every 3 seconds and a DHT11 not more
    //   often than every second. If a manual read is attempted too early, the
    //   status field of the data will indicate a timeout, and otherwise nothing
    //   bad should happen, except that the current thread may block for a short
    //   amount of time.
    // I am choosing to go with the second option here. This means I need to
    // manage the frequency with which this function is called myself.

    DHTXXD_manual_read(dht);
    auto const data{DHTXXD_data(dht)};

    return (data.status == DHT_GOOD)
      ? dht22{sample_sensor(clock), {data.temperature}, {data.humidity}}
      : dht22{};
  }

  auto setup_mhz19_io(auto const &pi, auto const &args) {
    if constexpr (std::tuple_size_v<typeof(args)> == 2)
      return io::Serial(pi, std::get<0>(args), std::get<1>(args));
    else return
      io::Serial(pi, std::get<0>(args), std::get<1>(args), std::get<2>(args));
  }

  mhz19 sample_mhz19(auto const &clock, auto const &serial) {
    // Resources on this sensor (MH-Z19, MH-Z19B, MH-Z19C):
    // https://revspace.nl/MHZ19
    // https://habr.com/ru/articles/401363/
    //
    // Regarding the calibration of this sensor: As the documentation is so
    // spread out among bad data sheets and experiments recorded in blog posts
    // and comments, for several different models, it's kinda hard to get a
    // solid idea of how I best do this.
    // First of all, it seems that the sensor does an automatic calibration
    // every 24h, which relies on an internal value that tracks something like
    // the minimum CO2 concentration and can be found in the 6th and maybe 7th
    // byte of the response to the standard read command (0x86).
    // There is a "zero point" and a "span" which can be calibrated. Calibration
    // can be triggered manually and the automatic calibration (also in some
    // places referred to as ABC, automatic baseline correction) can be turned
    // off.
    // There are critiques of the automatic calibration, which may lead to hard
    // jumps in the readings, but it seems this is especially the case in the
    // first couple days of operation and perhaps even more so if the sensor is
    // operated at 3.3V instead of 5V. Otherwise, maybe it's not that bad,
    // especially if the sensor is regularly exposed to 400ppm.
    // I don't fully understand the manual calibration, especially in regards to
    // what conditions need to be present when performing it. It looks like
    // there is a possibility of ruining the readings more or less forever if
    // doing this wrong.
    // So I guess I'll go with the "ABC" for now, as my sensors should be
    // exposed to atmospheric conditions at least semi-regularly. And then I'll
    // record the special value "U" in case it helps with correcting some issues
    // during data analysis.
    //
    // The detection range can be set to 1000, 2000, 3000, and 5000ppm. I reckon
    // a higher range comes with lower accuracy? As I don't know whether setting
    // this triggers any other kind of resets, I should probably do it
    // infrequently.

    char constexpr byte_start{0xff};
    char constexpr byte_sensor_number{0x01};

    std::array<char, 8> constexpr cmd_read{{byte_start, byte_sensor_number,
      0x86, 0x00, 0x00, 0x00, 0x00, 0x00}};

    serial_flush(serial);
    int const response{mhz19_send(serial, cmd_read)};
    if (response >= 0) {
      auto const maybe_packet{mhz19_receive(serial)};
      if (maybe_packet.has_value()) {
        auto const packet{maybe_packet.value()};
        float const co2_concentration{static_cast<float>(
          static_cast<std::uint32_t>(packet[2]) << 8u |
          static_cast<std::uint32_t>(packet[3]) << 0u)};
        float const temperature{packet[4] - 40.f};
        int const status{packet[5]};
        int const u0{packet[6]}, u1{packet[7]};
        return mhz19{sample_sensor(clock), {co2_concentration}, {temperature},
          status == 0 ? std::optional<int>{} : std::optional<int>{status},
          {u0}, {u1}};
      }
    }
    return mhz19{};
  }
} // namespace sensors

