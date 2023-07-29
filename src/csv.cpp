namespace io::csv {

  template <typename T>
  struct CSVWrapper {
    T const &x;
  };

  template <typename T>
  std::ostream& operator<<(std::ostream& out, CSVWrapper<T> const &x) {
    return out << x.x;
  }

  template <typename T>
  std::ostream& operator<<(std::ostream& out,
      CSVWrapper<std::optional<T>> const &x) {
    return x.x.has_value() ? (out << CSVWrapper<T>{x.x.value()})
                           : out << ""; // Still need to call `<<` e.g. for `setw`
  }

  std::ostream& operator<<(std::ostream& out, CSVWrapper<bool> const &x) {
    return out << (x.x ? cc::csv_true_string : cc::csv_false_string);
  }

  // NOTE: This outputs seconds since epoch. Not necessarily Unix time epoch.
  template <class Rep, std::intmax_t Num, std::intmax_t Denom>
  std::ostream& operator<<(std::ostream& out,
      CSVWrapper<std::chrono::duration<Rep, std::ratio<Num, Denom>>> const &x) {
    auto const original_width{out.width()};
    auto const original_flags{out.flags()};
    auto seconds{std::imaxdiv(x.x.count() * Num, Denom)};
    out << std::dec << std::setw(cc::timestamp_width) << seconds.quot;
    if constexpr (cc::timestamp_decimals > 0) {
      auto const original_fill{out.fill()};
      auto constexpr timestamp_den{static_cast<std::intmax_t>(
        util::power(10, cc::timestamp_decimals))};
      auto const fractional{
        std::imaxdiv(seconds.rem * timestamp_den, Denom).quot};
      out << "." << std::setw(cc::timestamp_decimals) << std::setfill('0')
        << fractional;
      out.fill(original_fill);
    }
    out.width(original_width);
    out.flags(original_flags);
    return out;
  }

}
