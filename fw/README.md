### Layout

L0

```
 --    Q     W    F      P      G     ·   ||    ·    J     L     U     Y     ;     --
  =    A     R    S      T      D     ·   ||    ·    H     N     E     I     O      '
 --    Z     X    C      V      B    DEL  ||   --    K     M     ,     .     /     --
 --   ESC   --   GUI   SHIFT  BSPC  CTRL  ||  ALT  SPACE  FN     -    --   ENTER   --
```

L1

```
  ~    !     @    UP     {      }     ·   ||    ·  PGUP    7     8     9     *      ~
  ~    #    LEFT DOWN  RIGHT    $     ·   ||    ·  PGDN    4     5     6     +      ~
  ~    [     ]    (      )      &     ~   ||    ~    `     1     2     3     \      ~
  ~    ~    INS  GUI   SHIFT  BSPC  CTRL  ||  ALT  SPACE   ~     .     0     =      ~
```

Hold `FN` for layer 1. `~` falls through to the base layer.
`--` is unmapped, `·` has no switch on the PCB.

Both thumb keys on the bottom row are mod-taps: tapped they send `TAB` / `ENTER`,
held they act as `CTRL` / `ALT`. A mod-tap resolves to its modifier as soon as any
other key goes down, or after 200 ms.
