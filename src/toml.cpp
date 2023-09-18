namespace io::toml {

  using indent_t = std::size_t;
  indent_t constexpr shiftwidth{2};

  template <typename T>
  struct TOMLWrapper {
    T &x;
    std::optional<std::string> comment;
    indent_t i;
    bool cond;

    TOMLWrapper<T>(T &&x, std::optional<std::string> comment = {},
        indent_t i = 0, bool cond = true) :
      x{x}, comment{comment}, i{i}, cond{cond} {}

    template <typename _T>
    TOMLWrapper<T>(T &x, TOMLWrapper<_T> const &y,
        std::optional<std::string> comment = {}) :
      x{x}, comment{comment}, i{y.i}, cond{false} {}
  };

  std::ostream& indent(std::ostream &out, indent_t const &i = shiftwidth,
      bool const cond = true) {
    if (cond) for (indent_t j{0}; j < i; ++j) out << ' ';
    return out;
  }

  template <typename T>
  std::ostream& indent(std::ostream &out, TOMLWrapper<T> const &x) {
    return indent(out, x.i, x.cond);
  }

  std::ostream& print_key(std::ostream &out, auto k) {
    bool const needs_quotes{false}; // Unsafe, but that's okay for now
    if (needs_quotes) out << '\"';
    out << k;
    if (needs_quotes) out << '\"';
    return out;
  }

  template <typename T>
  std::ostream& print_comment(std::ostream &out, TOMLWrapper<T> const &x) {
    if (x.comment.has_value()) out << " # " << x.comment.value();
    return out;
  }

  template <typename... Ts>
  std::ostream& operator<<(std::ostream& out,
      TOMLWrapper<const std::tuple<Ts...>> const xs) {
    indent(out, xs) << '[';
    bool is_first_entry{true};
    util::for_constexpr([&](auto const &x){
        if (is_first_entry) is_first_entry = false; else out << ", ";
        out << TOMLWrapper{x, xs};
      }, xs.x);
    return print_comment(out << ']', xs);
  }

  template <typename... Ts>
  std::ostream& operator<<(std::ostream& out,
      TOMLWrapper<std::tuple<Ts...>> const xs) {
    auto const & x{xs.x};
    return out << TOMLWrapper{x, xs, xs.comment};
  }

  template <std::ranges::range T>
  std::ostream& operator<<(std::ostream& out, TOMLWrapper<T> const xs)
      requires (not std::is_convertible_v<T, std::string_view>) {
    return out << TOMLWrapper(util::map_constexpr([](auto &x){
        return x;
      }, xs.x), xs.comment, xs.i, xs.cond);
  }

  template <typename T>
  std::ostream& operator<<(std::ostream& out, TOMLWrapper<T> const x) {
    auto const original_flags{out.flags()};
    indent(out, x);
    bool constexpr needs_quotes{not std::is_arithmetic_v<T>};
    bool constexpr use_hex{std::is_unsigned_v<T>};
    if (use_hex) out << std::hex << std::showbase;
    if (needs_quotes) out << '\"';
    out << x.x;
    if (needs_quotes) out << '\"';
    out.flags(original_flags);
    return print_comment(out, x);
  }

  std::ostream& operator<<(std::ostream &out,
      TOMLWrapper<std::nullptr_t> const x) {
    return print_comment(indent(out, x), x);
  }

  std::ostream& operator<<(std::ostream &out,
      TOMLWrapper<char const *> const x) {
    return out << TOMLWrapper{std::string{x.x}, x.comment, x.i, x.cond};
  }

  std::ostream& operator<<(std::ostream &out,
      TOMLWrapper<bool> const x) {
    return print_comment(indent(out, x) << (x.x ? "true" : "false"), x);
  }

  std::ostream& operator<<(std::ostream &out,
      TOMLWrapper<std::filesystem::path> const x) {
    return out << TOMLWrapper{x.x.generic_string(), x.comment, x.i, x.cond};
  }

  template <typename Clock, typename Duration>
  std::ostream& operator<<(std::ostream &out,
      TOMLWrapper<std::chrono::time_point<Clock, Duration>> const x) {
    // NOTE: At the moment, this is probably the most practical way of turning a
    // time point into a datetime string. This restricts the time points to be
    // of the system clock kind for now.
    auto const time{std::chrono::system_clock::to_time_t(x.x)};
    auto const gmtime{std::gmtime(&time)};
    return print_comment(indent(out, x) <<
      std::put_time(gmtime, "%Y-%m-%d %H:%M:%SZ"), x);
  }

  template <class Rep, std::intmax_t Num, std::intmax_t Denom>
  std::ostream& operator<<(std::ostream& out, TOMLWrapper<
      std::chrono::duration<Rep, std::ratio<Num, Denom>>> const &x) {
    return print_comment(indent(out, x) << io::csv::CSVWrapper<
      std::chrono::duration<Rep, std::ratio<Num, Denom>>>{x.x}, x);
  }

  template <typename K, typename V>
  std::ostream& operator<<(std::ostream& out,
      TOMLWrapper<std::pair<K, V>> const x) {
    auto const original_width{out.width(0)};
    print_key(indent(out, x), x.x.first) << " = ";
    out.width(original_width);
    return out << TOMLWrapper{x.x.second, x, x.comment} << "\n";
  }

  template <typename K, typename V>
  std::ostream& operator<<(std::ostream& out,
      TOMLWrapper<std::pair<K, std::optional<V>>> const x) {
    if (x.x.second.has_value()) {
      out << TOMLWrapper{std::make_pair(x.x.first, x.x.second.value()),
        x.comment, x.i, x.cond};
    }
    return out;
  }

}
