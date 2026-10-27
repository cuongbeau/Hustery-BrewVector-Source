#pragma once

// Copy this file to local_access.h and choose a per-device Wi-Fi AP password.
// local_access.h is intentionally excluded from Git.
#define BREWVECTOR_CREDENTIALS_CONFIGURED 0
#define BREWVECTOR_AP_PASSWORD "CHANGE_THIS_AP_PASSWORD"

#if !BREWVECTOR_CREDENTIALS_CONFIGURED
#error "Edit hmi/include/local_access.h and set BREWVECTOR_CREDENTIALS_CONFIGURED to 1"
#endif
