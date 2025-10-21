#pragma once

// Ce fichier corrige les conflits de définitions des méthodes HTTP
// entre ESPAsyncWebServer et le SDK ESP32 (inclus par WiFiManager).

#ifdef HTTP_DELETE
#undef HTTP_DELETE
#endif
#ifdef HTTP_GET
#undef HTTP_GET
#endif
#ifdef HTTP_HEAD
#undef HTTP_HEAD
#endif
#ifdef HTTP_POST
#undef HTTP_POST
#endif
#ifdef HTTP_PUT
#undef HTTP_PUT
#endif
#ifdef HTTP_PATCH
#undef HTTP_PATCH
#endif
#ifdef HTTP_OPTIONS
#undef HTTP_OPTIONS
#endif
