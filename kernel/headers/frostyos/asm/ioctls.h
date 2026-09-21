#ifndef _FROSTYOS_ASM_IOCTLS_H
#define _FROSTYOS_ASM_IOCTLS_H

#define TCGETS       0x5401
#define TCSETS       0x5402
#define TCSBRK       0x5409
#define TCFLSH       0x540B
#define TIOCSCTTY    0x540E
#define TIOCGPGRP    0x540F
#define TIOCSPGRP    0x5410
#define TIOCGWINSZ   0x5413
#define TIOCSWINSZ   0x5414
#define FIONREAD     0x541B
#define TIOCINQ      FIONREAD
#define TCSBRKP      0x5425


#endif /* _FROSTYOS_ASM_IOCTLS_H */