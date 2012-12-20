#if not "move_label" in vars()
#   define move_label "-"
#endif

#if not "following_error" in vars()
#   define following_error "-"
#endif

#define labels [move_label, following_error]
#define labels ' '.join([x for x in labels if x])

VA AA = 0 * 256 + 1 ' 0x0001: Version 0.1
LB CF
  PR AA," $labels"
E
