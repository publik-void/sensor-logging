namespace util {

  // `constexpr` power function, taken from https://stackoverflow.com/a/27271374
  template <typename T> constexpr T sqr(T a) { return a * a; }
  template <typename T> constexpr T power(T a, std::size_t n) {
      return n == 0 ? 1 : sqr(power(a, n / 2)) * (n % 2 == 0 ?  1 : a);
  }

  // NOTE: Maybe it would be possible to have one overloaded function name in
  // the below, where depending on whether the return type of the passed
  // function is void, the "loop" returns a tuple or not. I'm sure this kind of
  // functional compile-time type-polymorphic iteration is also present in some
  // library like Boost. But I'll not spend more time on this for now, as it
  // does what I need in its current state, so the priority of doing this more
  // elegantly is low.

  // `constexpr` (unrolled) for-loop
  template <auto n, auto i = std::size_t{0}, auto inc = std::size_t{1}, class F>
  void constexpr for_constexpr(F &&f) {
    if constexpr (i < n) {
      f(std::integral_constant<decltype(i), i>{});
      for_constexpr<n, i + inc, inc>(f);
    }
  }
  template <class F, class T, class... Ts>
  void constexpr for_constexpr(F &&f, T &&t, Ts &&...ts) {
    std::size_t constexpr n{std::tuple_size_v<std::decay_t<T>>};
    for_constexpr<n>([&](auto const i){
      f(std::get<i.value>(t), std::get<i.value>(ts)...);
    });
  }

  // `constexpr` for-loop with return values as tuple
  template <auto n, auto i = std::size_t{0}, auto inc = std::size_t{1}, class F>
  auto constexpr map_constexpr(F &&f) {
    if constexpr (i < n) {
      return std::tuple_cat(std::make_tuple(
        f(std::integral_constant<decltype(i), i>{})),
        map_constexpr<n, i + inc, inc>(f));
    } else return std::tuple<>{};
  }
  template <class F, class T, class... Ts>
  auto constexpr map_constexpr(F &&f, T &&t, Ts &&...ts) {
    std::size_t constexpr n{std::tuple_size_v<std::decay_t<T>>};
    return map_constexpr<n>([&](auto const i){
      return f(std::get<i.value>(t), std::get<i.value>(ts)...);
    });
  }

  // transposition (or "zipping") for tuples
  template <class T, class... Ts>
  auto constexpr tr(std::tuple<T, Ts...> const xss) {
    std::size_t constexpr n{std::tuple_size_v<std::decay_t<T>>};
    return map_constexpr<n>([&](auto const i){
        return map_constexpr([&](auto const &xs){
            return std::get<i.value>(xs);
          }, xss);
      });
  }

  template <typename tuple_t>
  auto constexpr as_array(tuple_t&& xs) {
    auto constexpr get_array{[](auto&& ... x){
      return std::array{std::forward<decltype(x)>(x) ... }; }};
    return std::apply(get_array, std::forward<tuple_t>(xs));
  }

  // Helper function to apply functions to optionals.
  // NOTE: `optional_apply` could perhaps be defined as a variadic template and
  // function, but let's keep it simple for now. C++23 also has something like
  // this, but I'm on C++20 for now.
  template <class F, class T>
  std::optional<T> optional_apply(F f, std::optional<T> const &x0) {
    if (x0.has_value()) return std::optional<T>{f(x0.value())};
    else return x0;
  }
  template <class F, class T>
  std::optional<T> optional_apply(F f,
      std::optional<T> const &x0, std::optional<T> const &x1) {
    if (x0.has_value()) {
      if (x1.has_value()) return std::optional<T>{f(x0.value(), x1.value())};
      else return x0;
    } else {
      if (x1.has_value()) return x1;
    else return x0;
    }
  }

  auto get_cmd_args(auto &flags, auto &opts, auto pos, auto const end) {
    char constexpr const * const opt_prefix{"--"};
    std::size_t constexpr opt_prefix_size{2};
    while (pos != end) {
      std::string const arg{*pos};
      if (not arg.starts_with(opt_prefix)) break;
      if (arg == opt_prefix) { ++pos; break; }
      auto const equal_sign_pos{arg.find_first_of('=')};
      std::string const arg_key{
        arg.substr(opt_prefix_size, equal_sign_pos - opt_prefix_size)};
      if (equal_sign_pos == decltype(arg)::npos) { // arg is a flag
        auto flag_itr = flags.find(arg_key);
        if (flag_itr == flags.end()) break;
        flag_itr->second = true;
      } else { // arg is an opt
        auto opt_itr = opts.find(arg_key);
        if (opt_itr == opts.end()) break;
        opt_itr->second = arg.substr(equal_sign_pos + 1);
      }
      ++pos;
    }
    return pos;
  }

  auto parse_arg_value(auto &&parser, auto &opts, auto const &key,
      auto const &default_value) {
    auto const &opt{opts[key]};
    if (not opt.has_value()) return default_value;
    try {
      return parser(opt.value());
    } catch (std::exception const &e) {
      if constexpr (cc::log_errors) std::cerr << log_error_prefix
        << "parsing option `" << key << "` (" << e.what() << "). Using default "
          "value." << std::endl;
      return default_value;
    }
  }

  long lfloor(auto num) {
    if constexpr (std::is_integral_v<decltype(num)>)
      return num;
    else
      return static_cast<long>(std::floor(num));
  }

} // namespace util

