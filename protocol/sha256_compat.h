#pragma once

#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

namespace BrewVectorSha256 {

inline int starts(mbedtls_sha256_context* context) {
#if MBEDTLS_VERSION_MAJOR >= 3
  return mbedtls_sha256_starts(context, 0);
#else
  return mbedtls_sha256_starts_ret(context, 0);
#endif
}

inline int update(mbedtls_sha256_context* context, const unsigned char* data,
                  size_t length) {
#if MBEDTLS_VERSION_MAJOR >= 3
  return mbedtls_sha256_update(context, data, length);
#else
  return mbedtls_sha256_update_ret(context, data, length);
#endif
}

inline int finish(mbedtls_sha256_context* context, unsigned char digest[32]) {
#if MBEDTLS_VERSION_MAJOR >= 3
  return mbedtls_sha256_finish(context, digest);
#else
  return mbedtls_sha256_finish_ret(context, digest);
#endif
}

}  // namespace BrewVectorSha256
