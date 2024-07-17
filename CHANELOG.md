# Changelog

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
