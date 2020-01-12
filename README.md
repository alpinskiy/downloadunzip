# DownloadUnzip [![Build status](https://ci.appveyor.com/api/projects/status/x0kpycvww4yb270k?svg=true)](https://ci.appveyor.com/project/alpinskiy/downloadunzip) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/e80cc3967700497ea4b69393816a1856)](https://www.codacy.com/manual/malpinskiy/downloadunzip?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=alpinskiy/downloadunzip&amp;utm_campaign=Badge_Grade)
```
Usage:
  download-unzip [Options] URL

Options:
  --login PATH
  POST request will be made before downloading.
  All received cookies will be sent with the following GET request.
  Should be relative to URL host.

  --login-data DATA
  x-www-form-urlencoded data to be sent with login POST request.

  --overwrite
  Overwrite local files.

  --sha256 HEXSTRING
  SHA-256 digest to verify file integrity.

  --save
  Save ZIP file to disk.

  --dryrun
  Operate as usual but write nothing to disk.

  --verbose
  Set verbose mode on.
```
