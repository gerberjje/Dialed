#pragma once

// Default preserves the original GC9A01 controller. Set to 1 for the touch board.
#ifndef DIALED_CONTROLLER_TOUCH
#define DIALED_CONTROLLER_TOUCH 0
#endif
#if DIALED_CONTROLLER_TOUCH != 0 && DIALED_CONTROLLER_TOUCH != 1
#error "DIALED_CONTROLLER_TOUCH must be 0 (mixer) or 1 (touch)"
#endif
