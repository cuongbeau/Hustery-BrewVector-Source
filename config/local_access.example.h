#pragma once

// Copy to local_access.h and set a unique recovery AP password for the Main.
// local_access.h is intentionally excluded from Git.
#define BREWVECTOR_MAIN_CREDENTIALS_CONFIGURED 0
#define BREWVECTOR_RECOVERY_AP_PASSWORD "CHANGE_THIS_RECOVERY_PASSWORD"

#if !BREWVECTOR_MAIN_CREDENTIALS_CONFIGURED
#error "Edit config/local_access.h and set BREWVECTOR_MAIN_CREDENTIALS_CONFIGURED to 1"
#endif
