# Changelog

## 0.77.1 (2025-01-16)

add pointCloudWaitForTrigger into ExecutionVerificationMode enum.

## 0.77.0 (2024-11-14)

fix mujinjson.h SaveJsonValue specializing stacked array values to bool

## 0.76.1 (2024-11-08)

fix mujinjson.h SaveJsonValue const char* variation.

## 0.76.0 (2024-11-03)

- fix mujinjson.h SaveJsonValue char[] variation.

## 0.75.0 (2024-11-02)

- mujinjson uses rapidjson::GenericValue with templates to allow for other allocators.

## 0.74.2 (2024-10-28)

- Move important code out of the ASSERT blocks in mujinjson

## 0.74.1 (2024-10-25)

- Handle serialization of NaN/Inf values in JSON

## 0.74.0 (2024-09-27)

- Enable non-virtual-dtor error detection and implement missing virtual destructors

## 0.73.4 (2024-08-07)

- Fix the logging error with 'No error', ensured that the curl function is executed only once in the CHECKCURLCODE macro.

## 0.73.3 (2024-07-30)

- Fix missing buffer reset in post helper function.

## 0.73.2 (2024-06-26)

- Fix invalid JSON string construction.

## 0.73.1 (2024-06-12)

- Add post helper function for formdata.

## 0.73.0 (2024-06-08)

- Remove `rotationmat` from `ResultOBB`. Use fixed size array on `ResultOBB`.

## 0.72.0 (2024-04-27)

- change `CreateLogEntries` to use `vector<LogEntry>` for memory optimization

## 0.71.0 (2024-03-26)

- Add objectType field in RegisterMinViableRegionInfo

## 0.70.2 (2023-11-21)

- Fix memory leak by incorrect usage of `curl_formfree`.

## 0.70.1 (2023-08-23)

- Add `CreateLogEntries` API support for uploading structured log entries with attachments.

## 0.69.1 (2023-08-23)

- Make `PickPlaceHistoryItem` json serializable.
