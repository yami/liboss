#ifndef OSS_ERROR_CODE_H
#define OSS_ERROR_CODE_H

/*
 * liboss error code is divided into several 'error zones'. Following
 * are currently defined zones:
 *   - -1
 *     unknown error
 *
 *   - 0
 *     when everything is fine, we return 0. 
 *
 *   - [1, 99]
 *     liboss zone.
 *
 *   - [100, 999]
 *     when there is an http error. However if http code is 2xx, we will still return 0.
 *
 *   - [1000, 1999]
 *     Code in this zone minus 1000 should be libcurl error code.
 *
 * we typedef int to oss_error_t, so that from a funtion signature, we can figure out
 * the return value is a liboss error code or not.
 *
 * Usage:
 *   - initialize error code to OSSE_UNKNOWN if need an unknown value
 *   - return OSSE_OK when everything is OK.
 *   - return http error code directly when it is an http error.
 *   - return OSS_CURLE(curl_error_code) when it is a curl error.
 */

typedef int oss_error_t;

#define OSSE_UNKNOWN    -1
#define OSSE_OK          0
#define OSSE_IMPOSSIBLE  1      /* liboss bug, or oss bug */
#define OSSE_NO_MEMORY   2
#define OSSE_IO_ERROR    3
#define OSSE_OPEN_FILE   4
#define OSSE_XML         5

#define OSS_EZONE_HTTP_BEG 100
#define OSS_EZONE_HTTP_END 1000

#define OSS_EZONE_CURL_BEG 1000
#define OSS_EZONE_CURL_END 2000

#define OSS_CURLE(curle) ((curle) + OSS_EZONE_CURL_BEG)

#endif
